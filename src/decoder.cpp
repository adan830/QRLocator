#include <iostream>
#include "zbar.h"
#include "decoder.h"

using namespace std;
using namespace zbar;

ImageScanner g_scanner;

void QR_CreateDecoder(void)
{
	g_scanner.set_config(ZBAR_NONE, ZBAR_CFG_ENABLE, 1);
    return;
}

void QR_Decode(unsigned char* data, int width, int height)
{
	Image image(width, height, "Y800", data, width * height);

    // scan the image for barcodes
    int n = g_scanner.scan(image);

    // extract results
    for(Image::SymbolIterator symbol = image.symbol_begin();
        symbol != image.symbol_end();
        ++symbol) {
        // do something useful with results
        cout << "decoded " << symbol->get_type_name()
             << " symbol \"" << symbol->get_data() << '"' << endl;
    }

    // clean up
    image.set_data(NULL, 0);

    return;
}


