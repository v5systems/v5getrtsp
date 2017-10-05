# v5getrtsp
# Project:
v5getrtsp is a tool allowing to get rtsp urls from onvif compatible cameras.

Output is formatted as JSON array.

The project is based on gSOAP toolkit:

https://sourceforge.net/projects/gsoap2/files/

http://www.cs.fsu.edu/~engelen/soap.html

https://www.genivia.com/products.html


# Building:
Build was tested on Ubuntu.
Just run "make" inside the project folder.

# Usage:
./v5getrtsp -l \<login\> -p \<password\> 	-i \<ip or host\> [-v \<output verbosity\>]

For example, ./v5getrtsp -l admin -p admin -i 10.10.101.101:8080
