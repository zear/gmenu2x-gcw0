#include "buttonbox.h"

#include "gmenu2x.h"
#include "iconbutton.h"

ButtonBox::ButtonBox(GMenu2X *gmenu2x) : gmenu2x(gmenu2x)
{
}

ButtonBox::~ButtonBox()
{
	clear();
}

void ButtonBox::add(IconButton *button)
{
	buttons.push_back(button);
}

void ButtonBox::clear()
{
	buttons.clear();
}

void ButtonBox::paint(Surface *s, unsigned int posX)
{
	for (auto button : buttons)
		posX = gmenu2x->drawButton(s, button, posX);
}

void ButtonBox::handleTS()
{
	for (auto button : buttons)
		button->handleTS();
}
