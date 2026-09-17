#include "platform.hpp"
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <algorithm>
#include <commdlg.h>
#include <stdexcept>
#include <windows.h>
namespace paint {
static HGLOBAL print_mode = nullptr;
static HGLOBAL print_names = nullptr;
void page_setup() {
    PAGESETUPDLGW setup{};
    setup.lStructSize = sizeof(setup);
    setup.hDevMode = print_mode;
    setup.hDevNames = print_names;
    setup.Flags = PSD_DEFAULTMINMARGINS;
    PageSetupDlgW(&setup);
    print_mode = setup.hDevMode;
    print_names = setup.hDevNames;
}
bool print_image(const Image& image) {
    Image flat;
    flat.reset(image.width, image.height);
    composite(flat, image, 0, 0);
    for (Color& pixel : flat.pixels) {
        std::swap(pixel.r, pixel.b);
    }
    PRINTDLGW dialog{};
    dialog.lStructSize = sizeof(dialog);
    dialog.hDevMode = print_mode;
    dialog.hDevNames = print_names;
    dialog.Flags = PD_RETURNDC | PD_NOPAGENUMS | PD_NOSELECTION | PD_USEDEVMODECOPIESANDCOLLATE;
    if (!PrintDlgW(&dialog)) {
        return false;
    }
    print_mode = dialog.hDevMode;
    print_names = dialog.hDevNames;
    HDC context = dialog.hDC;
    DOCINFOW document{};
    document.cbSize = sizeof(document);
    document.lpszDocName = L"Rainstar Paint picture";
    bool success = StartDocW(context, &document) > 0;
    if (success) {
        success = StartPage(context) > 0;
    }
    if (success) {
        int page_width = GetDeviceCaps(context, HORZRES);
        int page_height = GetDeviceCaps(context, VERTRES);
        double scale = std::min(static_cast<double>(page_width) / image.width,
                                static_cast<double>(page_height) / image.height);
        int width = static_cast<int>(image.width * scale);
        int height = static_cast<int>(image.height * scale);
        BITMAPINFO bitmap{};
        bitmap.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bitmap.bmiHeader.biWidth = image.width;
        bitmap.bmiHeader.biHeight = -image.height;
        bitmap.bmiHeader.biPlanes = 1;
        bitmap.bmiHeader.biBitCount = 32;
        bitmap.bmiHeader.biCompression = BI_RGB;
        SetStretchBltMode(context, HALFTONE);
        int result =
            StretchDIBits(context, (page_width - width) / 2, (page_height - height) / 2, width, height, 0, 0,
                          image.width, image.height, flat.pixels.data(), &bitmap, DIB_RGB_COLORS, SRCCOPY);
        success = result != GDI_ERROR && EndPage(context) > 0;
    }
    if (success) {
        success = EndDoc(context) > 0;
    } else {
        AbortDoc(context);
    }
    DeleteDC(context);
    return success;
}
} // namespace paint
