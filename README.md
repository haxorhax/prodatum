# **prodatum - E-MU Proteus sysex editor**

### Allows easy editing of presets for the E-MU Proteus family of synthesizers.
#### Specifically:
- Proteus 2000/2500 racks
- Command Station (MP-7, XL-7, PX-7)
- Audity 2000
<br>
<br>



# ***Build Process***

### **Build Process -- [ git, CMake, FLTK, portmidi ]** 
```bash
$ git clone https://github.com/haxorhax/prodatum.git
$ git clone https://github.com/haxorhax/fltk.git prodatum/lib/fltk
$ git clone https://github.com/haxorhax/portmidi.git prodatum/lib/portmidi
$ cd prodatum
$ cmake -S . -B build
$ cmake --build build --config Release
```
<br>
<br>


# ***Version Info***

## *Version 2.0.2 - 8/21/22*
> Resurrected repository.  Enhanced cmake build process, and added github action support.
###### Update provided by haxorhax (https://haxorhax.com)  
<br>

## *Versions 2.0.1 and older*
> Last updated on 1/31/2015.
###### Original design by Jan Mann (aka Jan Eidtmann, rdxesy@yahoo.de)  
#### Sourceforge repository:  https://sourceforge.net/projects/prodatum
###### *Copyright (C) 2014 by Jan Eidtmann*  


