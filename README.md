# Image-Fingerprinting
##### A compact way to implement reverse image-search using "fingerprinting" inspired by Shazam's audio fingerprinting algorithm. Fingerprints are taken from 50x50 regions of an image (adjustable) and are made insensitive to rotations, noise, distortion etc. by the averaging of pixel values. Additionally, a whole-image fingerprint is taken with [pHash](http://phash.org/) to enable the detection of resizes.
## Configuration / Setup
##### Step 1: Clone the repo
```git clone https://github.com/leoorshansky/image-fingerprinting```
##### Step 2: Install dependencies
Follow the directions to install [boost](https://www.boost.org/) for your operating system.
Also, install the included pHash source (on linux, do the following):
```cd pHash && ./configure && make && make install```
##### Step 3: Build
With make, simply type ```make``` and the executable will be built. With anything else, do what you normally do to build a C++ project.
##### Step 4: Run
Type ```./fingerprint --help``` to see the command syntax. Enjoy!