#include <iostream>
#include <fstream>
#include <iomanip>
#include <cmath>

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
    ifstream image(argv[1], ios_base::in | ios_base::binary);
    ofstream output(argv[2], ios_base::binary);
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
            uint32_t pixel_array[info_header.h][words];

            if (color_table_entrys != 0)
            {
                image.read((char *)&color_table, sizeof(color_table));
            }
            
            image.read((char *)&pixel_array, sizeof(pixel_array));
            //start proccessing
            for (int j = 0; j < info_header.h; j++)
            {
                for (int i = 0; i < words; i++)
                {
                    pixel_array[j][i] = (~pixel_array[j][i]);
                }
            }

            //end proccessing
            output.write((char*)&image_header, sizeof(Header));
            output.write((char*)&info_header, sizeof(BMP_info_header));
            if (color_table_entrys != 0)
            {
                output.write((char*)&color_table, sizeof(color_table));
            }
            output.write((char*)pixel_array, sizeof(pixel_array));
            image.close();
            cout << sizeof(image_header) << endl;
            cout << sizeof(info_header) << endl;
            cout << sizeof(color_table) << endl;
            cout << sizeof(pixel_array) << endl;
            cout << info_header.w << " " << info_header.h << endl;
            cout << round_bytes(info_header.w, info_header.bpp) << padding << endl;
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