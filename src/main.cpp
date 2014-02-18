#include <iostream>
#include <stb/stb_image.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb/stb_image_write.h>

#define FAILED_TO_LOAD_IMAGE 1
#define FAILED_TO_WRITE_IMAGE 2

#define DISTANCE_FIELD_DISTANCE 20

void showHelp(const char *msg)
{
    if (msg)
        std::cout << msg << "\n\n";

    std::cout << ("Usage:\n"
               "\t-h Help\n"
               "\t-i The input file\n"
               "\t-o The output file\n");
}

bool check(const void *arr, const std::string &errMsg, const std::string &successMsg)
{
    if (!arr)
        std::cerr << errMsg << "\n";
    else
        std::cout << successMsg << "\n";

    return arr;
}

bool checkPixel(unsigned char pixel[4])
{
    return pixel[0] || pixel[1] || pixel[2];
}

/* Assuming 4 channels */
bool checkNeighbors(unsigned char *pixels, const int &width, const int &height, const size_t &index, const int &distance)
{
    int channels = 4;
    size_t arrSize = width * height * channels;

    // TL -> TR
    for (size_t i = index - (distance * width * channels) - distance * channels; i < index - (distance * width * channels) + distance * channels; i += 4)
        if (i > 0 && i < arrSize && checkPixel(pixels + i))
            return true;

    // BL -> BR
    for (size_t i = index + (distance * width * channels) - distance * channels; i < index + (distance * width * channels) + distance * channels; i += 4)
        if (i > 0 && i < arrSize && checkPixel(pixels + i))
            return true;

    // TL -> BL
    for (size_t i = index - (distance * width * channels) - distance * channels; i < index + (distance * width * channels) - distance * channels;
         i += width * channels)
        if (i > 0 && i < arrSize && checkPixel(pixels +i))
            return true;

    // TR -> BR
    for (size_t i = index - (distance * width * channels) + distance * channels; i < index + (distance * width * channels) + distance * channels;
         i += width * channels)
        if (i > 0 && i < arrSize && checkPixel(pixels + i))
            return true;

    return false;
}

bool getNeighborOutlineArray(unsigned char *input, const int &w, const int &h, unsigned char *output)
{
    assert(w >= 0);
    assert(h >= 0);
    static const uint channels{4};
    size_t arraySize{uint(w) * uint(h) * channels};

    int passEdge{0};
    bool passToggle = false;
    for (size_t i{0}; i < arraySize; i += channels)
    {
        bool res = checkNeighbors(input, w, h, i, 1);
        bool me = checkPixel(input + i);
        if (res != me && !passToggle)
        {
            ++passEdge;
            passToggle = true;
        }
        else if (res == me)
            passToggle = false;

        output[i+0] = res == me ? 0x00  : 0xff;
        output[i+1] = res == me ? 0x00 : 0xff;
        output[i+2] = res == me ? 0x00 : 0xff;
        output[i+3] = 0xff;
    }
    return true;
}

bool getOr(unsigned char *input1, unsigned char *input2, const int &w, const int &h, unsigned char *output)
{
    assert(w >= 0);
    assert(h >= 0);
    static const uint channels{4};
    size_t arraySize{uint(w) * uint(h) * channels};
    for (size_t i{0}; i < arraySize; ++i)
        output[i] = input1[i] | input2[i];

    return true;
}

void filter(unsigned char *input, const int &w, const int &h, const int &rgba, unsigned char *output)
{
    char r{char((rgba & 0xff000000) >> 24)}, g{char((rgba & 0xff0000) >> 16)}, b{char((rgba & 0xff00) >> 8)}, a{char(rgba & 0xff)};
    assert(w >= 0);
    assert(h >= 0);
    static const uint channels{4};
    size_t arraySize{uint(w) * uint(h) * channels};
    for (size_t i{0}; i < arraySize; i += channels)
    {
        output[i] = input[i] & r;
        output[i+1] = input[i+1] & g;
        output[i+2] = input[i+2] & b;
        output[i+3] = input[i+3] & a;
    }
}

bool getDistanceField(unsigned char *input, const int &w, const int &h, unsigned char *output)
{
    assert(w >= 0);
    assert(h >= 0);
    static const uint channels{4};
    static char amount = 0x60;
    size_t arraySize{uint(w) * uint(h) * channels};

    for (size_t i = 0; i < arraySize; i += channels)
    {
        output[i] = 0xff;
        output[i+1] = 0xff;
        output[i+2] = 0xff;
        output[i+3] = 0x00;

        if (checkPixel(input+i))
        {
            output[i+3] = 0x80;
        }
        else
        {
            for (int j = 0; j < DISTANCE_FIELD_DISTANCE; ++j)
                if (checkNeighbors(input, w, h, i, j))
                {
                    float ratio{float(DISTANCE_FIELD_DISTANCE - j) / float(DISTANCE_FIELD_DISTANCE)};
                    output[i+3] += ratio * amount;
                    break;
                }
        }
    }
    return true;
}

int main(int argc, char **argv)
{
    if (argc != 7)
    {
        if (argc < 2)
            showHelp(0);
        else
            showHelp("Wrong number of arguments!");
        return 0;
    }

    std::string in_file, out_file, minification_str;

    for (int i = 0; i < argc - 1; ++i)
    {
        if (!strcmp(argv[i], "-i"))
            in_file = std::string(argv[i+1]);
        else if (!strcmp(argv[i], "-o"))
            out_file = std::string(argv[i+1]);
        else if (!strcmp(argv[i], "-m"))
            minification_str = std::string(argv[i+1]);
    }


    if (in_file.empty() || out_file.empty() || minification_str.empty())
    {
        showHelp("You have to specify an input file, an output file and a minification factor!");
        return 0;
    }

    int minification = std::stoi(minification_str);
    if (minification <= 0)
    {
        showHelp("The minification factor has to be a value > 0");
        return 0;
    }

    int width, heigt, channels;
    unsigned char *pixels = stbi_load(in_file.c_str(), &width, &heigt, &channels, STBI_rgb_alpha);
    if (!check(pixels, "Failed to load image", std::string{"Loaded image: "} + in_file))
        return FAILED_TO_LOAD_IMAGE;

    size_t arrSize{uint(width) * uint(heigt) * channels};
    unsigned char *edgePixels = new unsigned char[arrSize];
    memset(edgePixels, 0, arrSize);
    getNeighborOutlineArray(pixels, width, heigt, edgePixels);

    unsigned char *redOriginal = new unsigned char[arrSize];
    unsigned char *fill = new unsigned char[arrSize];
    memset(fill, 0, arrSize);
    filter(pixels, width, heigt, 0xff0000ff, redOriginal);
    getOr(redOriginal, edgePixels, width, heigt, fill);

    unsigned char *distanceField = new unsigned char[arrSize];
    memset(distanceField, 0, arrSize);
    getDistanceField(pixels, width, heigt, distanceField);


    std::cout << "Done with filter\n";

    int s1 = stbi_write_png(std::string("e_" + std::string(out_file)).c_str(), width, heigt, channels, fill, 0);
    int s2 = stbi_write_png(out_file.c_str(), width, heigt, channels, distanceField, 0);
    delete edgePixels;

    if (!(s1 && s2))
    {
        std::cerr << "Failed to write: " << out_file << "\nTerminating...\n";
        return FAILED_TO_WRITE_IMAGE;
    }

    std::cout << "Wrote: " << out_file << "\n";
    std::cout << "Done!\n";
    return 0;
}