///////////////////////////////////////////////////////////////////////////////
//
// wxFormBuilder - A Visual Dialog Editor for wxWidgets.
// Copyright (C) 2005 José Antonio Hurtado
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
//
// Written by
//   José Antonio Hurtado - joseantonio.hurtado@gmail.com
//   Juan Antonio Ortega  - jortegalalmolda@gmail.com
//
///////////////////////////////////////////////////////////////////////////////

#include "bitmaps.h"

#include <algorithm>

#include <wx/intl.h>
#include <wx/log.h>
#include <wx/window.h>

#include <plugin_interface/default.xpm>
#include <common/xmlutils.h>


std::map<wxString, wxBitmap> AppBitmaps::m_bitmaps;


wxBitmap AppBitmaps::GetBitmap(const wxString& iconname, Size size)
{
    wxBitmap bmp;
    if (auto bitmap = m_bitmaps.find(iconname); bitmap != m_bitmaps.end()) {
        bmp = bitmap->second;
    } else {
        bmp = m_bitmaps["unknown"];
    }
    if (size != Size::Default && (bmp.GetWidth() != GetPixelSize(size) || bmp.GetHeight() != GetPixelSize(size))) {
        bmp = ScaleImage(bmp.ConvertToImage(), size);
    }

    return bmp;
}

void AppBitmaps::LoadBitmaps(const wxString& filepath, const wxString& iconpath)
{
    m_bitmaps.insert_or_assign("unknown", wxBitmap(default_xpm));

    auto doc = XMLUtils::LoadXMLFile(filepath, true);
    if (!doc) {
        wxLogError(_("%s: Failed to open file"), filepath);

        return;
    }
    if (doc->Error()) {
        wxLogError(doc->ErrorStr());

        return;
    }
    const auto* root = doc->FirstChildElement("icons");
    if (!root) {
        wxLogError(_("%s: Invalid root node"), filepath);

        return;
    }

    for (const auto* icon = root->FirstChildElement("icon"); icon; icon = icon->NextSiblingElement("icon")) {
        auto name = XMLUtils::StringAttribute(icon, "name");
        if (name.empty()) {
            wxLogError(_("%s: Empty icon name"), filepath);
            continue;
        }
        auto file = XMLUtils::StringAttribute(icon, "file");
        if (file.empty()) {
            wxLogError(_("%s: Empty icon path"), filepath);
            continue;
        }

        m_bitmaps.insert_or_assign(name, wxBitmap(iconpath + file, wxBITMAP_TYPE_ANY));
    }
}


namespace
{
// multiplies the colours with the alpha, or divides them by it
void PremultiplyAlpha(wxImage& image, bool undo)
{
    auto* rgb = image.GetData();
    const auto* alpha = image.GetAlpha();
    for (int i = 0, count = image.GetWidth() * image.GetHeight(); i < count; ++i, rgb += 3) {
        for (int c = 0; c < 3; ++c) {
            if (undo) {
                rgb[c] = alpha[i] == 0 ? 0 : std::min(255, rgb[c] * 255 / alpha[i]);
            } else {
                rgb[c] = rgb[c] * alpha[i] / 255;
            }
        }
    }
}
}  // namespace


wxBitmap AppBitmaps::ScaleImage(const wxImage& image, Size size)
{
    // the scale factor keeps the size in DIPs, otherwise wxBitmapBundle scales the bitmap again
    const auto pixelSize = GetPixelSize(size);
    const auto scale = static_cast<double>(pixelSize) / static_cast<int>(size);
    if (image.GetWidth() == pixelSize && image.GetHeight() == pixelSize) {
        return wxBitmap(image, wxBITMAP_SCREEN_DEPTH, scale);
    }

    // resampling blends the colour of transparent pixels into the edges, a mask colour would show up as dark border,
    // so convert the mask to alpha and resample with premultiplied alpha
    auto scaled = image.Copy();
    if (scaled.HasMask()) {
        scaled.InitAlpha();
    }
    const auto hasAlpha = scaled.HasAlpha();
    if (hasAlpha) {
        PremultiplyAlpha(scaled, false);
    }
    scaled.Rescale(pixelSize, pixelSize, wxIMAGE_QUALITY_HIGH);
    if (hasAlpha) {
        PremultiplyAlpha(scaled, true);
    }
    return wxBitmap(scaled, wxBITMAP_SCREEN_DEPTH, scale);
}


int AppBitmaps::GetPixelSize(Size size)
{
    // the sizes are in DIPs, scale them with the DPI of the main display
    return wxWindow::FromDIP(static_cast<int>(size), nullptr);
}
