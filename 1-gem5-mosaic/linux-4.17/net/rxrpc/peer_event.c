/* Peer event handling, typically ICMP messages.
 *
 * Copyright (C) 2007 Red Hat, Inc. All Rights Reserved.
 * Written by David Howells (dhowells@redhat.com)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */

#include <linux/module.h>
#include <linux/net.h>
#include <linux/skbuff.h>
#include <linux/errqueue.h>
#include <linux/udp.h>
#include <linux/in.h>
#include <linux/in6.h>
#include <linux/icmp.h>
#include <net/sock.h>
#include <net/af_rxrpc.h>
#include <net/ip.h>
#include "ar-internal.h"

static void rxrpc_store_error(struct rxrpc_peer *, struct sock_exterr_skb *);

/*
 * Find the peer associated with an ICMP packet.
 */
static struct rxrpc_peer *rxrpc_lookup_peer_icmp_rcu(struct rxrpc_local *local,
						     const struct sk_buff *skb,
						     struct sockaddr_rxrpc *srx)
{
	struct sock_exterr_skb *serr = SKB_EXT_ERR(skb);

	_enter("");

	memset(srx, 0, sizeof(*srx));
	srx->transport_type = local->srx.transport_type;
	srx->transport_len = local->srx.transport_len;
	srx->transport.family = local->srx.transport.family;

	/* Can we see an ICMP4 packet on an ICMP6 listening socket?  and vice
	 * versa?
	 */
	switch (srx->transport.family) {
	case AF_INET:
		srx->transport.sin.sin_port = serr->port;
		switch (serr->ee.ee_origin) {
		case SO_EE_ORIGIN_ICMP:
			_net("Rx ICMP");
			memcpy(&srx->transport.sin.sin_addr,
			       skb_network_header(skb) + serr->addr_offset,
			       sizeof(struct in_addr));
			break;
		case SO_EE_ORIGIN_ICMP6:
			_net("Rx ICMP6 on v4 sock");
			memcpy(&srx->transport.sin.sin_addr,
			       skb_network_header(skb) + serr->addr_offset + 12,
			       sizeof(struct in_addr));
			break;
		default:
			memcpy(&srx->transport.sin.sin_addr, &ip_hdr(skb)->saddr,
			       sizeof(struct in_addr));
			break;
		}
		break;

#ifdef CONFIG_AF_RXRPC_IPV6
	case AF_INET6:
		srx->transport.sin6.sin6_port = serr->port;
		switch (serr->ee.ee_origin) {
		case SO_EE_ORIGIN_ICMP6:
			_net("Rx ICMP6");
			memcpy(&srx->transport.sin6.sin6_addr,
			       skb_network_header(skb) + serr->addr_offset,
			       sizeof(struct in6_addr));
			break;
		case SO_EE_ORIGIN_ICMP:
			_net("Rx ICMP on v6 sock");
			srx->transport.sin6.sin6_addr.s6_addr32[0] = 0;
			srx->transport.sin6.sin6_addr.s6_addr32[1] = 0;
			srx->transport.sin6.sin6_addr.s6_addr32[2] = htonl(0xffff);
			memcpy(srx->transport.sin6.sin6_addr.s6_addr + 12,
			       skb_network_header(skb) + serr->addr_offset,
			       sizeof(struct in_addr));
			break;
		default:
			memcpy(&srx->transport.sin6.sin6_addr,
			       &ipv6_hdr(skb)->saddr,
			       sizeof(struct in6_addr));
			break;
		}
		break;
#endif

	default:
		BUG();
	}

	return rxrpc_lookup_peer_rcu(local, srx);
}

/*
 * Handle an MTU/fragmentation problem.
 */
static void rxrpc_adjust_mtu(struct rxrpc_peer *peer, struct sock_exterr_skb *serr)
{
	u32 mtu = serr->ee.ee_info;

	_net("Rx ICMP Fragmentation Needed (%d)", mtu);

	/* wind down the local interface MTU */
	if (mtu > 0 && peer->if_mtu == 65535 && mtu < peer->if_mtu) {
		peer->if_mtu = mtu;
		_net("I/F MTU %u", mtu);
	}

	if (mtu == 0) {
		/* they didn't give us a size, estimate one */
		mtu = peer->if_mtu;
		if (mtu > 1500) {
			mtu >>= 1;
			if (mtu < 1500)
				mtu = 1500;
		} else {
			mtu -= 100;
			if (mtu < peer->hdrsize)
				mtu = peer->hdrsize + 4;
		}
	}

	if (mtu < peer->mtu) {
		spin_lock_bh(&peer->lock);
		peer->mtu = mtu;
		peer->maxdata = peer->mtu - peer->hdrsize;
		spin_unlock_bh(&peer->lock);
		_net("Net MTU %u (maxdata %u)",
		     peer->mtu, peer->maxdata);
	}
}

/*
 * Handle an error received on the local endpoint.
 */
void rxrpc_error_report(struct sock *sk)
{
	struct sock_exterr_skb *serr;
	struct sockaddr_rxrpc srx;
	struct rxrpc_local *local = sk->sk_user_data;
	struct rxrpc_peer *peer;
	struct sk_buff *skb;

	_enter("%p{%d}", sk, local->debug_id);

	skb = sock_dequeue_err_skb(sk);
	if (!skb) {
		_leave("UDP socket errqueue empty");
		return;
	}
	rxrpc_new_skb(skb, rxrpc_skb_rx_received);
	serr = SKB_EXT_ERR(skb);
	if (!skb->len && serr->ee.ee_origin == SO_EE_ORIGIN_TIMESTAMPING) {
		_leave("UDP empty message");
		rxrpc_free_skb(skb, rxrpc_skb_rx_freed);
		return;
	}

	rcu_read_lock();
	peer = rxrpc_lookup_peer_icmp_rcu(local, skb, &srx);
	if (peer && !rxrpc_get_peer_maybe(peer))
		peer = NULL;
	if (!peer) {
		rcu_read_unlock();
		rxrpc_free_skb(skb, rxrpc_skb_rx_freed);
		_leave(" [no peer]");
		return;
	}

	trace_rxrpc_rx_icmp(peer, &serr->ee, &srx);

	if ((serr->ee.ee_origin == SO_EE_ORIGIN_ICMP &&
	     serr->ee.ee_type == ICMP_DEST_UNREACH &&
	     serr->ee.ee_code == ICMP_FRAG_NEEDED)) {
		rxrpc_adjust_mtu(peer, serr);
		rcu_read_unlock();
		rxrpc_free_skb(skb, rxrpc_skb_rx_freed);
		rxrpc_put_peer(peer);
		_leave(" [MTU update]");
		return;
	}

	rxrpc_store_error(peer, serr);
	rcu_read_unlock();
	rxrpc_free_skb(skb, rxrpc_skb_rx_freed);

	/* The ref we obtained is passed off to the work item */
	__rxrpc_queue_peer_error(peer);
	_leave("");
}

/*
 * Map an error report to error codes on the peer record.
 */
static void rxrpc_store_error(struct rxrpc_peer *peer,
			      struct sock_exterr_skb *serr)
{
	struct sock_extended_err *ee;
	int err;

	_enter("");

	ee = &serr->ee;

	err = ee->ee_errno;

	switch (ee->ee_origin) {
	case SO_EE_ORIGIN_ICMP:
		switch (ee->ee_type) {
		case ICMP_DEST_UNREACH:
			switch (ee->ee_code) {
			case ICMP_NET_UNREACH:
				_net("Rx Received ICMP Network Unreachable");
				break;
			case ICMP_HOST_UNREACH:
				_net("Rx Received ICMP Host Unreachable");
				break;
			case ICMP_PORT_UNREACH:
				_net("Rx Received ICMP Port Unreachable");
				break;
			case ICMP_NET_UNKNOWN:
				_net("Rx Received ICMP Unknown Network");
				break;
			case ICMP_HOST_UNKNOWN:
				_net("Rx Received ICMP Unknown Host");
				break;
			default:
				_net("Rx Received ICMP DestUnreach code=%u",
				     ee->ee_code);
				break;
			}
			break;

		case ICMP_TIME_EXCEEDED:
			_net("Rx Received ICMP TTL Exceeded");
			break;

		default:
			_proto("Rx Received ICMP error { type=%u code=%u }",
			       ee->ee_type, ee->ee_code);
			break;
		}
		break;

	case SO_EE_ORIGIN_NONE:
	case SO_EE_ORIGIN_LOCAL:
		_proto("Rx Received local error { error=%d }", err);
		err += RXRPC_LOCAL_ERROR_OFFSET;
		break;

	case SO_EE_ORIGIN_ICMP6:
	default:
		_proto("Rx Received error report { orig=%u }", ee->ee_origin);
		break;
	}

	peer->error_report = err;
}

/*
 * Distribute an error that occurred on a peer
 */
void rxrpc_peer_error_distributor(struct work_struct *work)
{
	struct rxrpc_peer *peer =
		container_of(work, struct rxrpc_peer, error_distributor);
	struct rxrpc_call *call;
	enum rxrpc_call_completion compl;
	int error;

	_enter("");

	error = READ_ONCE(peer->error_report);
	if (error < RXRPC_LOCAL_ERROR_OFFSET) {
		compl = RXRPC_CALL_NETWORK_ERROR;
	} else {
		compl = RXRPC_CALL_LOCAL_ERROR;
		error -= RXRPC_LOCAL_ERROR_OFFSET;
	}

	_debug("ISSUE ERROR %s %d", rxrpc_call_completions[compl], error);

	spin_lock_bh(&peer->lock);

	while (!hlist_empty(&peer->error_targets)) {
		call = hlist_entry(peer->error_targets.first,
				   struct rxrpc_call, error_link);
		hlist_del_init(&call->error_link);
		rxrpc_see_call(call);

		if (rxrpc_set_call_completion(call, compl, 0, -error))
			rxrpc_notify_socket(call);
	}

	spin_unlock_bh(&peer->lock);

	rxrpc_put_peer(peer);
	_leave("");
}

/*
 * Add RTT information to cache.  This is called in softirq mode and has
 * exclusive access to the peer RTT data.
 */
void rxrpc_peer_add_rtt(struct rxrpc_call *call, enum rxrpc_rtt_rx_trace why,
			rxrpc_serial_t send_serial, rxrpc_serial_t resp_serial,
			ktime_t send_time, ktime_t resp_time)
{
	struct rxrpc_peer *peer = call->peer;
	s64 rtt;
	u64 sum = peer->rtt_sum, avg;
	u8 cursor = peer->rtt_cursor, usage = peer->rtt_usage;

	rtt = ktime_to_ns(ktime_sub(resp_time, send_time));
	if (rtt < 0)
		return;

	/* Replace the oldest datum in the RTT buffer */
	sum -= peer->rtt_cache[cursor];
	sum += rtt;
	peer->rtt_cache[cursor] = rtt;
	peer->rtt_cursor = (cursor + 1) & (RXRPC_RTT_CACHE_SIZE - 1);
	peer->rtt_sum = sum;
	if (usage < RXRPC_RTT_CACHE_SIZE) {
		usage++;
		peer->rtt_usage = usage;
	}

	/* Now recalculate the average */
	if (usage == RXRPC_RTT_CACHE_SIZE) {
		avg = sum / RXRPC_RTT_CACHE_SIZE;
	} else {
		avg = sum;
		do_div(avg, usage);
	}

	peer->rtt = avg;
	trace_rxrpc_rtt_rx(call, why, send_serial, resp_serial, rtt,
			   usage, avg);
}

/*
 * Perform keep-alive pings with VERSION packets to keep any NAT alive.
 */
void rxrpc_peer_keepalive_worker(struct work_struct *work)
{
	struct rxrpc_net *rxnet =
		container_of(work, struct rxrpc_net, peer_keepalive_work);
	struct rxrpc_peer *peer;
	unsigned long delay;
	ktime_t base, now = ktime_get_real();
	s64 diff;
	u8 cursor, slot;

	base = rxnet->peer_keepalive_base;
	cursor = rxnet->peer_keepalive_cursor;

	_enter("%u,%lld", cursor, ktime_sub(now, base));

next_bucket:
	diff = ktime_to_ns(ktime_sub(now, base));
	if (diff < 0)
		goto resched;

	_debug("at %u", cursor);
	spin_lock_bh(&rxnet->peer_hash_lock);
next_peer:
	if (!rxnet->live) {
		spin_unlock_bh(&rxnet->peer_hash_lock);
		goto out;
	}

	/* Everything in the bucket at the cursor is processed this second; the
	 * bucket at cursor + 1 goes now + 1s and so on...
	 */
	if (hlist_empty(&rxnet->peer_keepalive[cursor])) {
		if (hlist_empty(&rxnet->peer_keepalive_new)) {
			spin_unlock_bh(&rxnet->peer_hash_lock);
			goto emptied_bucket;
		}

		hlist_move_list(&rxnet->peer_keepalive_new,
				&rxnet->peer_keepalive[cursor]);
	}

	peer = hlist_entry(rxnet->peer_keepalive[cursor].first,
			   struct rxrpc_peer, keepalive_link);
	hlist_del_init(&peer->keepalive_link);
	if (!rxrpc_get_peer_maybe(peer))
		goto next_peer;

	spin_unlock_bh(&rxnet->peer_hash_lock);

	_debug("peer %u {%pISp}", peer->debug_id, &peer->srx.transport);

recalc:
	diff = ktime_divns(ktime_sub(peer->last_tx_at, base), NSEC_PER_SEC);
	if (diff < -30 || diff > 30)
		goto send; /* LSW of 64-bit time probably wrapped on 32-bit */
	diff += RXRPC_KEEPALIVE_TIME - 1;
	if (diff < 0)
		goto send;

	slot = (diff > RXRPC_KEEPALIVE_TIME - 1) ? RXRPC_KEEPALIVE_TIME - 1 : diff;
	if (slot == 0)
		goto send;

	/* A transmission to this peer occurred since last we examined it so
	 * put it into the appropriate future bucket.
	 */
	slot = (slot + cursor) % ARRAY_SIZE(rxnet->peer_keepalive);
	spin_lock_bh(&rxnet->peer_hash_lock);
	hlist_add_head(&peer->keepalive_link, &rxnet->peer_keepalive[slot]);
	rxrpc_put_peer(peer);
	goto next_peer;

send:
	rxrpc_send_keepalive(peer);
	now = ktime_get_real();
	goto recalc;

emptied_bucket:
	cursor++;
	if (cursor >= ARRAY_SIZE(rxnet->peer_keepalive))
		cursor = 0;
	base = ktime_add_ns(base, NSEC_PER_SEC);
	goto next_bucket;

resched:
	rxnet->peer_keepalive_base = base;
	rxnet->peer_keepalive_cursor = cursor;
	delay = nsecs_to_jiffies(-diff) + 1;
	timer_reduce(&rxnet->peer_keepalive_timer, jiffies + delay);
out:
	_leave("");
}
