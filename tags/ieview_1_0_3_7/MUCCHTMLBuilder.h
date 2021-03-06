class MUCCHTMLBuilder;

#ifndef MUCCHTMLBUILDER_INCLUDED
#define MUCCHTMLBUILDER_INCLUDED

#include "HTMLBuilder.h"

class MUCCHTMLBuilder:public HTMLBuilder
{
protected:
    virtual void loadMsgDlgFont(int i, LOGFONTA * lf, COLORREF * colour);
	virtual char *timestampToString(DWORD dwFlags, time_t check);
public:
    MUCCHTMLBuilder();
	void buildHead(IEView *, IEVIEWEVENT *event);
	void appendEvent(IEView *, IEVIEWEVENT *event);
	void clear();
};

#endif
