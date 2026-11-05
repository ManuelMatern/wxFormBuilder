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

#include "model/objectbase.h"
#include "rad/appdata.h"
#include "utils/typeconv.h"
#include "visualeditor.h"


BEGIN_EVENT_TABLE(VObjEvtHandler, wxEvtHandler)
EVT_LEFT_DOWN(VObjEvtHandler::OnLeftClick)
EVT_RIGHT_DOWN(VObjEvtHandler::OnRightClick)
EVT_PAINT(VObjEvtHandler::OnPaint)
EVT_SET_CURSOR(VObjEvtHandler::OnSetCursor)
END_EVENT_TABLE()


VObjEvtHandler::VObjEvtHandler(wxWindow* win, PObjectBase obj)
{
    m_window = win;
    m_object = obj;
}

namespace
{
bool IsVisualObject(wxWindow* window)
{
    // The handler doesn't need to be the topmost one
    for (auto* handler = window->GetEventHandler(); handler; handler = handler->GetNextHandler()) {
        if (dynamic_cast<VObjEvtHandler*>(handler)) {
            return true;
        }
    }
    return false;
}

wxWindow* FindDisabledVisualObject(wxWindow* parent, const wxPoint& screenPos)
{
    for (auto* child : parent->GetChildren()) {
        if (!child->IsShown() || child->IsTopLevel() || !child->GetScreenRect().Contains(screenPos)) {
            continue;
        }
        if (!child->IsEnabled() && IsVisualObject(child)) {
            return child;
        }
        // Descend into enabled children as well, e.g. wxStaticBox is transparent for mouse clicks on wxMSW,
        // so clicks on its disabled children are delivered to the window below it
        if (auto* found = FindDisabledVisualObject(child, screenPos)) {
            return found;
        }
    }
    return nullptr;
}
}  // namespace

bool VObjEvtHandler::RedirectToDisabledChild(wxWindow* parent, wxMouseEvent& event)
{
    // Only handle clicks the parent received itself, not the ones that are forwarded from its children
    if (event.GetEventObject() != parent) {
        return false;
    }

    const wxPoint screenPos = parent->ClientToScreen(event.GetPosition());
    auto* child = FindDisabledVisualObject(parent, screenPos);
    if (!child) {
        return false;
    }

    wxMouseEvent childEvent(event);
    childEvent.SetPosition(child->ScreenToClient(screenPos));
    childEvent.SetEventObject(child);
    child->HandleWindowEvent(childEvent);
    return true;
}

void VObjEvtHandler::OnLeftClick(wxMouseEvent& event)
{
    if (RedirectToDisabledChild(m_window, event)) {
        return;
    }

    PObjectBase obj = m_object.lock();

    if (obj) {
        if (obj->GetObjectTypeName() == wxT("ribbonbar")) {
            if (AppData()->GetSelectedObject() != obj)
                AppData()->SelectObject(obj);

            event.Skip();
            return;
        }

        if (AppData()->GetSelectedObject() != obj) {
            AppData()->SelectObject(obj);

            // this event is always redirected to parent (aui)toolbar even if its tool was clicked
            // so it is important to skip the event to select clicked tool later in "common"
            // plugin.
            if (obj->GetObjectTypeName() == wxT("toolbar") || obj->GetObjectTypeName() == wxT("toolbar_form"))
                event.Skip();
        } else {
            // *!* Event should be skipped only in the case of the object selected
            // is the same that the object clicked. You will experiment rare things
            // in other case.
            event.Skip();
        }
    }

    m_window->ClientToScreen(&event.m_x, &event.m_y);
    m_window->GetParent()->ScreenToClient(&event.m_x, &event.m_y);
    ::wxPostEvent(m_window->GetParent(), event);
}

void VObjEvtHandler::OnRightClick(wxMouseEvent& event)
{
    // show context menu associated with the widget if any

    PObjectBase obj = m_object.lock();

    if (obj) {
        if (obj->GetPropertyAsInteger(wxT("context_menu")) != 0) {
            PObjectBase menu;

            for (size_t i = 0; i < obj->GetChildCount(); i++) {
                if (obj->GetChild(i)->GetObjectTypeName() == wxT("menu")) {
                    menu = obj->GetChild(i);
                    break;
                }
            }

            if (menu)
                m_window->PopupMenu(DesignerWindow::GetMenuFromObject(menu), event.GetPosition());
        } else
            event.Skip();
    }

    m_window->ClientToScreen(&event.m_x, &event.m_y);
    m_window->GetParent()->ScreenToClient(&event.m_x, &event.m_y);
    ::wxPostEvent(m_window->GetParent(), event);
}

void VObjEvtHandler::OnPaint(wxPaintEvent& event)
{
    /*	PObjectBase wo = boost::shared_dynamic_cast<ObjectBase>(m_object.lock());
            if (wo->IsContainer())
            { TODO: what this check is for? */
    wxWindow* aux = m_window;
    while (!aux->IsKindOf(CLASSINFO(DesignerWindow))) aux = aux->GetParent();
    DesignerWindow* dsgnWin = (DesignerWindow*)aux;
    if (dsgnWin->GetActivePanel() == m_window) {
        wxPaintDC dc(m_window);
        dsgnWin->HighlightSelection(dc);
    }
    /*	}*/
    event.Skip();
}

void VObjEvtHandler::OnSetCursor(wxSetCursorEvent& event)
{
    wxCoord x = event.GetX(), y = event.GetY();
    m_window->ClientToScreen(&x, &y);
    m_window->GetParent()->ScreenToClient(&x, &y);
    wxSetCursorEvent sce(x, y);
    ::wxPostEvent(m_window->GetParent(), sce);
}
