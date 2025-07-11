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

![UI](/UI.jpg) 

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
Schematic drawings are found under each subsystem folder in the Hardware folder (i.e. Scale and Mainboard).

### Mainboard
The mainboard contains the ESP32C3 and interfaces for programmin and power (USB-C), scale and servo as well as reset and boot buttons. The system is easily programmed using Arduino through the USB-C interface.

![UI](/Hardware/Mainboard/top.jpg) 

![UI](/Hardware/Mainboard/bottom.jpg) 

### Scale
The scale board contains an HX711 to interface with common load cells. No programming required - but calibration from the main controller is required at commissioning.

![UI](/Hardware/Scale/top.jpg) 

![UI](/Hardware/Scale/bottom.jpg) 

## Software
The software is not prettified but has the functionality needed. It utilizes existing libraries to provide and host the UI. Scale calibration is incorporated in the software, making it easy commission a new system. Find the latest software in the software folder.

---

## Disclaimer

**Use at your own risk!** Improper connections could potentially damage hardware. The hardware and software have been tested with the listed devices without issues.

