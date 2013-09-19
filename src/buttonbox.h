#ifndef __BUTTONBOX_H__
#define __BUTTONBOX_H__

#include <vector>

class GMenu2X;
class IconButton;

class ButtonBox
{
public:
	ButtonBox(GMenu2X *gmenu2x);
	~ButtonBox();

	void add(IconButton *button);
	void clear();

	void paint(unsigned int posX);
	void handleTS();

private:
	std::vector<IconButton*> buttons;
	GMenu2X *gmenu2x;
};

#endif
