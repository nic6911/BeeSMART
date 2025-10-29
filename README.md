# BeeSMART

If you like then please

[!["Buy Me A Coffee"](https://www.buymeacoffee.com/assets/img/custom_images/orange_img.png)](https://bmc.link/nic6911w)


This work is licensed under a  
[Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License][cc-by-nc-sa].

[![CC BY-NC-SA 4.0][cc-by-nc-sa-image]][cc-by-nc-sa]

[cc-by-nc-sa]: http://creativecommons.org/licenses/by-nc-sa/4.0/  
[cc-by-nc-sa-image]: https://licensebuttons.net/l/by-nc-sa/4.0/88x31.png  
[cc-by-nc-sa-shield]: https://img.shields.io/badge/License-CC%20BY--NC--SA%204.0-lightgrey.svg

---

## Buy one, Køb en, Kaufen 
€200 + shipping from DK

Write me an email:

![Contact](/contact.png)

---

## What is it?
Honey filling system controlled from your phone, tablet or PC !

![Setup](/setup.jpg) 

![Scale](/scale.jpg) 

![UI](/UI.png) 

---

## Dansk
Honning tappemaskine til den lille produktion.
WIFI baseret med brugerinterface på pc, tablet eller smartphone uden en app.

### Video
Demo: https://youtu.be/7Y-k81tILdg
Montage: https://youtu.be/ojsv-gZ6waY

### Specs
Leverer inden for +/- 5g. Doseringsmængden kan indstilles mellem 50g og 20kg (Vær opmærksom på vægtens max belastning som kan være væsentligt mindre end 20kg !).
Se manual for funktionalitet: https://github.com/nic6911/BeeSMART/blob/main/BeeSMART_manual.pdf

--- 

## Deutsch
Honig Abfüllsystem für den kleinen Produktion.
BeeSMART basiert sich auf WiFi und bietet eine Benutzeroberfläche 
entweder auf einem PC, Tablet oder Smartphone, ohne dass eine APP benötigt wird.

### Video
Demo: https://youtu.be/7Y-k81tILdg
Montage: https://youtu.be/ojsv-gZ6waY

### Specs
Es ist möglich mit einer Genauigkeit von +/- 5g zu erreichen. Der Menge kann zwischen 50g und 20kg eingestellt werden (Achte auf die maximale Belastung der Waage, die möglicherweise deutlich weniger als 20 kg beträgt !).
Für detaillierte Beschreibung von Funktionalität bitte lesen sie das Manual hier: https://github.com/nic6911/BeeSMART/blob/main/DE_BeeSMART_manual.pdf

---

## English
Honey filling machine for the small production.
WIFI based with user interface on pc, tablet or smartphone without the need for an app.

### Video
Demo: https://youtu.be/7Y-k81tILdg
Assembly: https://youtu.be/ojsv-gZ6waY

### Specs
An accuracy of +/- 5g is achievable. The amount to fill can be selected in the range from 50g to 20kg (Be mindful of the scale's maximum load, which may be significantly less than 20kg !).
See the user manual for more information: https://github.com/nic6911/BeeSMART/blob/main/EN_BeeSMART_manual.pdf

---

## Mechanical parts
Find the STL files for 3D printing in the STL's folder. The 2nd revision fits the updated hardware that is also found in the hardware folder.

## Hardware
The hardware is specifically designed for the project. RJ45 is utilized for connecting scale and mainboard, giving a seamless and robust connection. Power is provided from a 3A 5V USB-C power supply while the servo has its dedicated connector on the main board.
Schematic drawings, production files and source files are found under each subsystem folder in the Hardware folder (i.e. Scale and Mainboard).

### Mainboard
The mainboard contains the ESP32C3 and interfaces for programmin and power (USB-C), scale and servo as well as reset and boot buttons. The system is easily programmed using Arduino through the USB-C interface.

![UI](/Hardware/Mainboard/top.jpg) 

![UI](/Hardware/Mainboard/bottom.jpg) 

### Scale
The scale board contains an HX711 to interface with common load cells. No programming required - but calibration from the main controller is required at commissioning.

![UI](/Hardware/Scale/top.jpg) 

![UI](/Hardware/Scale/bottom.jpg) 

## Construction
There are a number of parts to acquire and some to 3D print. All the parts for printing are found in the Mechanical folder. You'll need one of each BUT only one of the brackets (either 40 or 53mm) and one of the main controller housing (40 or 53mm).

Besides the 3D printed parts some of the main components you'll need are:

- 2 x Ikea Bamboo lids Art no 103.819.09
- 1 x Standard size servo like e.g., MG996R
- 1 x Load cell
- 1 x Mainboard
- 1 x Scale board
- 1 x 5V 3A USB-C power supply
- 1 x RJ45 cable

The scale is constructed using two Ikea Bamboo lids Art no 103.819.09, the 3D printed encapsulation, electronics and a load cell that can be found almost anywhere. To assemble the scale, two holes must be drilled in each bamboo lid, where one side of the load cell is typically M4 and the other side M5. To mount them you'll need 2 x M4x25 and 2 x M5x30.

![UI](/Mechanical/scale_perspective.png) 

![UI](/Mechanical/load_cell.png) 

The "controller" is the unit that contains the Mainboard and Servo. The servo is a standard size servo like the MG996R. To mount that in the 3D printed housing you'll need 4 x M3x25 and 4 x M3 nuts.

![UI](/Mechanical/render.PNG) 

All the remaining screws and nuts required are listed on the BOM in the manual.

## Software
The software is not prettified but has the functionality needed. It utilizes existing libraries to provide and host the UI. Scale calibration is incorporated in the software, making it easy commission a new system. Find the latest software in the software folder.

### HiL
The HiL folder contains a hardware-in-loop software package. I have updated it to make it relatively plug & play - simply just upload the BeeSMART software to one board and the "combinedsimulator" to another and connect scale and servo pins 1:1. This now gives you a superb opportunity for tuning the software on different simulated viscosities as a semi-advanced model has been implemented for simulating the behavior of the bucket and honey.
![HiL](/Software/HiL/HiL_setup.jpg) 
![HiL](/Software/HiL/HIL.PNG) 

---

## Disclaimer

**Use at your own risk!** Improper connections could potentially damage hardware. The hardware and software have been tested with the listed devices without issues.

