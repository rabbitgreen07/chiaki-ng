#ifndef CHIAKI_GUI_SCREENSHOT_H
#define CHIAKI_GUI_SCREENSHOT_H

#include <QImage>
#include <QString>

struct AVFrame;

// Converts a decoded video frame (software or hardware) into an 8-bit RGB
// QImage. HDR (PQ / HLG, BT.2020) frames are tone-mapped to SDR so the result
// looks right in a normal image viewer. Returns a null image on failure and
// fills *error if provided.
QImage ChiakiFrameToImage(const AVFrame *frame, QString *error = nullptr);

// Directory screenshots are written to: <Pictures>/chiaki-ng
QString ChiakiScreenshotDirectory();

#endif // CHIAKI_GUI_SCREENSHOT_H
