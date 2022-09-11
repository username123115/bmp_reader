# bmp_reader
This project was made for gaining more familiarity with the bmp file format and is very heavily work in progress.
I've used this alongside my matrix library to be able to perform a wide variety of affine transformations to my images.
Currently the project contains two cpp files: reader.cpp, and test_reader.cpp. Most recent development in my project has been in test_reader.cpp.
Most features are broken as of now and the file can't support files with bits per pixel less than 8, but it can reliably apply matrix transformations to most other images.
