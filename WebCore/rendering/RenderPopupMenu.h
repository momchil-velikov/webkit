/*
 * This file is part of the popup menu implementation for <select> elements in WebCore.
 *
 * Copyright (C) 2006 Apple Computer, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */

#ifndef RENDER_POPUPMENU_H
#define RENDER_POPUPMENU_H

#include "RenderBlock.h"
#include "RenderMenuList.h"

namespace WebCore {

class HTMLOptionElement;
class HTMLOptGroupElement;

class RenderPopupMenu : public RenderBlock {
public:
    RenderPopupMenu(Node*);
    virtual ~RenderPopupMenu() {}
    
    virtual const char* renderName() const { return "RenderPopupMenu"; }

    virtual void clear() = 0;
    virtual void populate();
    virtual void showPopup(const IntRect&, FrameView*, int index) = 0;
    
    RenderMenuList* getRenderMenuList() { return static_cast<RenderMenuList*>(parent() ? parent()->parent() : 0); }

protected:
    virtual void addSeparator() = 0;
    virtual void addGroupLabel(HTMLOptGroupElement*) = 0;
    virtual void addOption(HTMLOptionElement*) = 0;

};

}

#endif
