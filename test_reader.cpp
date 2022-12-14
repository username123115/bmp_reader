#include <iostream>
#include <fstream>
#include <iomanip>
#include <cmath>
#include "matrix.h"

#pragma pack (push, 1)
struct Header 
{
    uint16_t signature;
    uint32_t size;
    uint16_t r1;
    uint16_t r2;
    uint32_t offset;
};

#pragma pack(push, 1)
struct BMP_info_header 
{
    uint32_t h_size;
    uint32_t w;
    uint32_t h;
    uint16_t planes;
    uint16_t bpp;
    uint32_t compression;
    uint32_t size; //image size
    uint32_t xppm;
    uint32_t yppm;
    uint32_t color_table_colors;
    uint32_t import_colors;
};
int max(int, int);
int get_padding(int, int);
int round_bytes(int, int);
//potentially declare some variables as constants to avoid confusion in the future.
void read_to_matrix(uint32_t, uint32_t, uint16_t, int, Matrix<uint32_t> &, ifstream &); //takes an empty matrix with specified dimensions and writes to it, file pointer must be at pixel array
void write_from_matrix(uint32_t, uint32_t, uint16_t, int, Matrix<uint32_t> &, ofstream &); //takes image matrix with dimensions specified and writes from it, file pointer must be at pixel array
void apply_transformation(uint16_t, Matrix<double> &, Matrix<uint32_t> &, Matrix<uint32_t> &); //takes end matrix and applys inverse of transform matrix before interpolating, changing image
void apply_test_watermark(Matrix<uint32_t> &, uint16_t);
void apply_rotation(uint16_t, double, Matrix<uint32_t> &, Matrix<uint32_t> &, double, double);
void bring_attention();

uint32_t get_default(unsigned, unsigned, Matrix<uint32_t> &, unsigned);
uint32_t billinear_interpolation(double, double, uint16_t, Matrix<uint32_t> &);
uint32_t linear_interpolation(double, double, uint16_t, Matrix<uint32_t> &);

uint32_t get_default(unsigned x, unsigned y, Matrix<uint32_t> &mat, unsigned d = 0)
{
    unsigned maxw = mat.getRows();
    unsigned maxh = mat.getCols();
    if ((0 <= x) && (x < maxw - 1))
    {
        if ((0 <= y) && (y < maxh - 1))
        {
            return mat(x, y);
        }
    }
    return d;
}
uint32_t linear_interpolation(double x, double y, uint16_t bpp, Matrix<uint32_t> & image)
{
    int x_high = floor(x);
    int y_low = floor(y);
    double vhigh = x - x_high;
    double hleft = y - y_low;
    int xnear = x_high; //top left
    int ynear = y_low;
    if (vhigh > 0.5)
    {
        xnear += 1;
    }
    if (hleft > 0.5)
    {
        ynear += 1;
    }
    return get_default(xnear, ynear, image);

}

//given the coordinates of output mapped into the input, find a approximation for the value it should be
uint32_t billinear_interpolation(double x, double y, uint16_t bpp, Matrix<uint32_t> &image)
{
    int x_high = floor(x); //x = 0 is the highest point on the image
    int y_low = floor(y);
    //weights are: vertical (high / low), horizontal (left / right)
    double vhigh = x - x_high;
    double vlow = 1 - vhigh;
    double hleft = y - y_low;
    double hright = 1 - hleft;  
    uint32_t ll, lr, ul, ur;
    int bits_per_color = bpp / 4; //only applicable to bpp >= 8
    uint32_t chunk = (pow(2, bits_per_color)) - 1;
    uint32_t red_mask = chunk << (bits_per_color * 3);
    uint32_t green_mask = chunk << (bits_per_color * 2);
    uint32_t blue_mask = chunk << (bits_per_color * 1);
    uint32_t alpha_mask = chunk;
    uint32_t color_masks[] = {red_mask, green_mask, blue_mask, alpha_mask};

    uint32_t result;
    double sum;

    unsigned maxw = image.getCols();
    unsigned maxh = image.getRows();

    ll = get_default(x_high + 1, y_low, image);
    lr = get_default(x_high + 1, y_low + 1, image);
    ul = get_default(x_high, y_low, image);
    ur = get_default(x_high, y_low + 1, image);

    for (int i = 0; i < 4; i++)
    {
        sum = ((ll & color_masks[i]) >> (3 - i)) * vhigh * hright; //remove unneccesary bits and shift right to get values represneted by target bits
        sum += ((lr & color_masks[i]) >> (3 - i)) * vhigh * hleft;
        sum += ((ul & color_masks[i]) >> (3 - i)) * vlow * hright;
        sum += ((ur & color_masks[i]) >> (3 - i)) * vlow * hleft;
        // result << bits_per_color; operation doesn't actually change result and is throwing a compiler warning lol
        result += round(sum);
        sum = 0;
    }
    // result = (result & color_masks[1]);

    
    return result;
    // return round(ll);
    // return 0;


}

void read_to_matrix(uint32_t w, uint32_t h, uint16_t bpp, int padding, Matrix<uint32_t> &image, ifstream &file)
{
    int reads_per_cycle = 1;
    if (bpp < 8)
    {
        reads_per_cycle = 8 / bpp; 
    }
    uint32_t read_col_count = w;
    if (reads_per_cycle != 1)
    {
        read_col_count = round_bytes(w, bpp);
    }
    uint32_t read_mask = pow(2, bpp) - 1;
    int bytes = bpp / 8;
    if (bytes < 1) {bytes = 1;}

    // accessing rows gives x coordinates while columns give y coordinates (cartesian plane rotated 90 degrees clockwise)
    for (int i = h - 1; i >= 0; i--) //read goes from bottom to top so we have to fill matrix from x = h-1 to x = 0 make sure to switch from incrementing to de-incrementing i
    {
        uint32_t padding_buffer = 0;
        for (int j = 0; j < read_col_count; j++)
        {
            uint32_t content = 0;
            file.read((char*)&content, bytes);
            
            // if (!(content == 255) && !(content == 254)) {bring_attention();} 

            //should have no effect for bpp > 8 because pixel will be set to zero, not effecting the read mask while the loop only runs once
            for (int pixel = reads_per_cycle - 1; pixel >= 0; pixel--)
            {
                uint32_t pixel_data = (content & (read_mask << (pixel * bpp))) >> (pixel * bpp);
                if (!(pixel_data == 1)) {bring_attention();}
                //bpp > 8 will not be effected by this as it will be accessing (i, j)
                image(i, j * reads_per_cycle + reads_per_cycle - 1 - pixel) = pixel_data;
            } //pixel (j, i) in the pixel array is assigned to matrix(j, i) instead of (i, j)
            
        }
        file.read((char*)&padding_buffer, padding);
    }
}
void write_from_matrix(uint32_t w, uint32_t h, uint16_t bpp, int padding, Matrix<uint32_t> &image, ofstream &file)
{
    int writes_per_cycle = 1;
    if (bpp < 8)
    {
        writes_per_cycle = 8 / bpp; 
    }
    uint32_t count = w;
    if (writes_per_cycle != 1)
    {
        count = round_bytes(w, bpp);
    }
    int bytes_out = bpp / 8;
    if (bytes_out < 1) {bytes_out = 1;}

    // if bpp less then one pack (8 / bpp) pixels = one byte into each write, this also writes an even amount of bytes, helping with padding
    for (int i = h - 1; i >= 0; i--) //write from bottom of matrix to top of matrix
    {

        for (int j = 0; j < count; j++)
        {
            uint32_t contents = 0;
            for (int pixel = 0; pixel < writes_per_cycle; pixel++)
            {
                contents = contents << bpp;
                contents += (image(i, j * writes_per_cycle + pixel)); //rows in the matrix represent x values instead of y values
                // contents << ((*image_matrix)(i, j + pixel) >> (info_header.bpp)); //pixel pixels into contents before writing, maximum 8 pixels for 1 bpp images 
            }
            file.write((char *)&contents, bytes_out);
        }
        for (int k = 0; k < padding; k++)
        {
            char zeros = 0;
            file.write((char*)&zeros, sizeof(char));
        }
    }
}
void apply_transformation(uint16_t bpp, Matrix<double> &transformation, Matrix<uint32_t> &original, Matrix<uint32_t> &change)
{
    Matrix<double> transform(3, 3, 0.0);
    if (transformation.getCols() != transformation.getRows())
    {
        cout << "Matrix must have same number of rows and columns" << endl;
        for (int i = 0; i < 3; i++)
        {
            transform(i, i) = 1;
        }
    }
    if (!((transformation.getCols() == 2) or (transformation.getRows() == 3)))
    {
        cout << "Matrix must have 2 or 3 rows and columns" << endl;
        for (int i = 0; i < 3; i++)
        {
            transform(i, i) = 1;
        }
    }
    
    
    if ((transformation.getCols() == 2) && (transformation.getRows() == 2))
    {
        for (int i = 0; i < 2; i++)
        {
            for (int j = 0; j < 2; j++)
            {
                transform(i, j) = transformation(i, j);
            }
        }
        transform(2, 2) = 1;
    }
    if ((transformation.getCols() == 3) && (transformation.getRows() == 3))
    {
        for (int i = 0; i < 3; i++)
        {
            for (int j = 0; j < 3; j++)
            {
                transform(i, j) = transformation(i, j);
            }
        }
    }


    Matrix<double> test = transform.get_inverse();
    // Matrix<double> inverse_transform = transformation.get_inverse();
    for (int i = 0; i < change.getRows(); i++)
    {
        for (int j = 0; j < change.getCols(); j++)
        {
            Matrix<double> out_coordinates(1, 3, 0.0);
            out_coordinates(0, 0) = i; //x coordinate
            out_coordinates(0, 1) = j; //y coordinate
            out_coordinates(0, 2) = 1; //For moving the images a 3 dimensional matrix is used
            // Matrix<double> in_coordinates = out_coordinates * inverse_transform;
            Matrix<double> in_coordinates = out_coordinates * test;
            double in_x = in_coordinates(0, 0);
            double in_y = in_coordinates(0, 1);
            change(i, j) = linear_interpolation(in_x, in_y, bpp, original);
            // (*output_matrix)(i, j) = (*image_matrix)(in_y, in_x);
        }
    }
    std::cout << "applied transformation: " << std::endl;
    transform.print();
}
void apply_rotation(uint16_t bpp, double angle, Matrix<uint32_t> &original, Matrix<uint32_t> & change, double x_proportion = 0, double y_proportion = 0)
{
    double theta = angle * 2 * M_PI / 360;
    Matrix<double> transform(3, 3, 0.0);
    Matrix<double> pivot_point(1, 3, 0.0);
    Matrix<double> pivot_out(1, 3, 0.0);
    double cos_theta = cos(theta);
    double sin_theta = sin(theta);
    transform(2, 2) = 1;
    transform(0, 0) = cos_theta;
    transform(1, 1) = cos_theta;
    transform(0, 1) = (-1) * sin_theta;
    transform(1, 0) = sin_theta;


    pivot_point(0, 0) = original.getRows() * x_proportion - 1;
    pivot_point(1, 0) = original.getCols() * y_proportion - 1;
    pivot_point(2, 0) = 1;
    pivot_out = (pivot_point * transform);
    transform(2, 0) = pivot_out(0, 0);
    transform(2, 1) = pivot_out(1, 0);



    apply_transformation(bpp, transform, original, change);

}
void apply_test_watermark(Matrix<uint32_t> &image, uint16_t bpp = 1)
{
    if ((image.getCols() >= 25) && (image.getRows() >= 10))
    {
        for (int i = 0; i < 25; i++)
        {
            for (int j = 0; j < 10; j++)
            {
                image(i, j) = pow(2, bpp) / 2;
            }
        }
    }
}
void bring_attention() {
    static uint32_t non_full = 0;
    non_full += 1;
}

int max(int a, int b) 
{
    if (a > b) {
        return a;
    }
    else {
        return b;
    }
}
/*I assume that when size of one image row does not make a full byte 
e.g 3 pixels in 1 bpp that the remainder still takes up space of a 
byte and as such padding still stays an integer amount of bytes
*/
int get_padding(int w, int bpp)
{
    //get rounded to higher byte count of row (should only apply to 1-4 bpp images)

    int bytes_rounded = round_bytes(w, bpp);
    int padding = 4 - bytes_rounded % 4;
    if (padding == 4)
    {
        padding = 0;
    }
    return padding;
}
int round_bytes(int w, int bpp)
{
    int extra = 0;
    if (((w * bpp) % 8) != 0)
    {
        extra = 8 - (w * bpp) % 8; 
    }
    int bytes_rounded = (w * bpp + extra) / 8;
    return bytes_rounded;
}

using namespace std;
int main(int argc, char* argv[]) 
{
    ifstream image("24bpp2.bmp", ios_base::in | ios_base::binary);
    ofstream output("bitmap_output.bmp", ios_base::binary);
    if (!image.is_open()) 
    {
        cout << "Image did not open" << endl;
        return 1;
    }   
    else 
    {
        Header image_header;
        BMP_info_header info_header;
        image.read((char *)&image_header, sizeof(Header));
        if (image_header.signature == 0x4d42) 
        {
            image.read((char *)&info_header, sizeof(info_header));
            
            int color_table_entrys = (image_header.offset - 12 - info_header.h_size) / 4; //values start from 54 to offset and each entry contains 4 bytes
            //check if there are color tables
            //char test[0];
            char color_table[color_table_entrys][4]; 
            
            
            //pixel data to be stored in 32 bit words
            int padding = get_padding(info_header.w, info_header.bpp);
            int words = (round_bytes(info_header.w, info_header.bpp) + padding) / 4;
            if (color_table_entrys != 0)
            {
                image.read((char *)&color_table, sizeof(color_table));
            }
            

            //reading to a matrix
            Matrix<uint32_t> *image_matrix = new Matrix<uint32_t>(info_header.h, info_header.w, 0); //x goes up and down and y goes left to right (not like regular coordinates)
            Matrix<uint32_t> *output_matrix = new Matrix<uint32_t>(info_header.h, info_header.w, 0);
            Matrix<double> transformation("transformation.txt");
            Matrix<double> shift("move.txt");
            // transformation = transformation * shift;

            read_to_matrix(info_header.w, info_header.h, info_header.bpp, padding, (*image_matrix), image);

            //proccessing in the middle
            apply_transformation(info_header.bpp, transformation, *image_matrix, *output_matrix);
            // apply_rotation(info_header.bpp, 35, *image_matrix, *output_matrix, 0, 0);

            //writing to the output
            output.write((char*)&image_header, sizeof(Header));
            output.write((char*)&info_header, sizeof(BMP_info_header));

            if (color_table_entrys != 0)
            {
                output.write((char*)&color_table, sizeof(color_table));
            }

            write_from_matrix(info_header.w, info_header.h, info_header.bpp, padding, (*output_matrix), output); //changed output matrix to image_matrix for testing

            //clean up
            image.close();
            output.close();
            delete image_matrix;
            delete output_matrix;
            return 0;
        }
        else 
        {
            cout << "Wrong file format" << endl;
            image.close();
            return 1;
        }
    }

}