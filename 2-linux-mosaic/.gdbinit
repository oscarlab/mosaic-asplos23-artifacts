set $lastcs = -1
set $amd64 = -1

file linux/vmlinux

define hook
  if $amd64 == -1
    if $cs == 8 || $cs == 27
      if $lastcs != 8 && $lastcs != 27 && $cs == 8
        set architecture i386
      end
      x/i $pc
    else
      if $lastcs == -1 || $lastcs == 8 || $lastcs == 27 || $cs == 0
        set architecture i8086
      end
      printf "[%4x:%4x] ", $cs, $eip
    end
    set $lastcs = $cs
  end
end

define x64
  set architecture i386:x86-64:intel
  set $amd64 = 1
end

define ls
  layout split
  fs cmd
end

define lr
  layout reg
  fs cmd
end

define rr
  info reg
end

define pp
  x/512xg (0xffff880000000000 + 0x$arg0 * 0x1000)
end

define il
  info locals
end

define ia
  info args
end

define pc
  display/i $pc
end

define ppc
  x/i $pc
end

define rpc
  disable display 1
end

define db
  disable $arg0 
end

target remote localhost:1234

b get_level1_bucket
