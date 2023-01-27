**Vivado and Verilog Installation on Windows**

1. Go to the following link https://www.xilinx.com/support/download/index.html/content/xilinx/en/downloadNav/vivado-design-tools/archive.html
2. Click on 2020.2 and download the Xilinx Unified Installer 2020.2: Windows Self Extracting Web Installer (EXE)
3. You will need to have or register for an account with Xilinx using an email address ending in ".edu"
4. Open the installer and click "Download and Install Now"
5. Accept the license agreements and select "Vivado" as the product to install
6. On the next screen, choose "Vivado HL WebPACK" as the edition
7. Under "Design Tools" make sure "Vivado Design Suite" is checked with "Vivado" and "Vitis HLS" underneath it both also checked and "DocNav" also checked.
8. Under "Devices" the only thing that should be checked is "Artix-7" under "7 Series" under "Production Devices"
9. Under "Installation Options" have "Install Cable Drivers" checked
10. You can use the default options on the following screen for the location of the installed software
11. Click "Install"
12. To launch Vivado after installation, there should have been a desktop shortcut called "Vivado 2020.2" downloaded that you can open (not "Vivado HLS", this is something different). If you do not see a desktop shortcut to Vivado, you can navigate to the install location (usually C:\Xilinx\Vivado\2020.2), then go into bin, and double-click vivado.bat

**Using Verilog in Vivado to Replicate Experiment**

1. Launch Vivado
2. Click "Create New Project" and click "Next"
3. Enter a name for your new project like "myVerilogProject" and click "Next"
4. Choose "RTL Project" and click "Next"
5. Select the following options: 
Product Category: All     Speed Grade: -1
Family: Artix-7           Temp Grade: All Remaining
Package: csg324
6. Select Part "xc7a100tcsg324-1" from the available options and click "Next"
7. Click "Finish"
8. You have now created your Verilog project, it is time to add all the needed source, constraint, and memory files. On the left under "Project Manager" select "Add Sources"
9. Select "Add or create design sources" and click "Next". Click on "Add Files" repeatedly to add multiple design sources. For this analysis we will use the source files for tabulation hashing with 4 hash seeds. Add "tabulationHash4.sv", "staticTable4.sv", "table1_initfile.mem", "table2_initfile.mem", "table3_initfile.mem", "table4_initfile.mem", "table5_initfile.mem". Click on "Next" and "Finish"
10. On the left under "Project Manager" select "Add Sources" again
11. Click "Add or create constraints"
12. Click on "Add Files" and add "clk.xdc". Click on "Next" and "Finish"
13. We have added all the necessary files needed to proceed with timing analysis. Under "Sources"->"Design Sources" double check "tabulationHash4.sv" is in bold. If it is not, right click on it and select "Set as Top"
14. Under "Project Manager" -> "Synthesis" select "Run Synthesis". Click on "OK" to use the default options. Synthesis may take a few minutes to complete 
15. Once synthesis is complete you will be alerted of this. On the alert select "Run Implementation" then "OK". Running implementation may also take a few minutes
16. Once implementation is complete you will be alerted of this. On the alert, select "View Reports". These reports will contain the timing analysis and resource usage of the Verilog design. Under "Route Design" you should find a "Timing Summary - Route Design" file. This file will should state the worst slack and total violation is -0.155ns. Since the design was synthesized using a clock with a period of 2ns, this means the minimum operational clock period of the Verilog design is 2ns + .155ns = 2.155ns which implies a maximum operational clock frequency of 464 MHz. Resource utlization can be found in the tabulationHash4_utilization_placed.rpt in reports

**Acknowledgment**

A special thanks to Dr. Montek Singh and his course materials for COMP 541 Digital Logic and Design, taught at the University of North Carolina at Chapel Hill, for much of the information concerning getting Vivado installed and a project setup
