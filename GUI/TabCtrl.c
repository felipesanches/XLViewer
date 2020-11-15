/****************************************************************************\
*            
*     FILE:     TABCTRL.C
*
*     PURPOSE:  Code for managing tab control property page dialogs C file.
*
*     COMMENTS: This source is distributed in the hope that it will be useful,
*               but WITHOUT ANY WARRANTY; without even the implied warranty of
*               MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
*
*     FUNCTIONS:
*      EXPORTS: 
*               New_TabControl()			- Initialize a TABCTRL struct, create tab page dialogs
*                                             and start the message loop
*               TabControl_Destroy()		- Destroy the tab page dialogs and
*                                             free the list of pointers to the dialogs.
*      EXPORTED
*      VIA POINTER:
*               CenterTabPage()				- Center the tab page in the tab control's display area
*               StretchTabPage()			- Stretch the tab page in the tab control's display area
*               Notify()                    - Handle WM_NOTIFY messages
*      LOCALS:
*               TabControl_GetClientRect()	- Return a Client rectangle for the tab control under
*                                             every possible configuration (tabs, buttons vert etc.)
*               TabCtrl_OnKeyDown()         - Handle key presses in the tab control (but not the tab pages)
*               TabPage_OnCommand()			- Forward commands to ParentProc.
*               TabPage_OnLButtonDown()		- Handle WM_LBUTTONDOWN (tab page dialog)
*               TabPage_OnSize()			- Handle WM_SIZE (tab page dialog)
*               TabPage_OnInitDialog()		- Handle WM_INITDIALOG (tab page dialog)
*               TabPage_DlgProc()			- Window Procedure for the tab pages (dialogs)
*               TabPageMessageLoop()		- Monitor and respond to user keyboard input and system messages (tab page dialog)
*               TabCtrl_OnSelChanged()      - A tab has been pressed, Handle TCN_SELCHANGE notification
*
*     Copyright 2006 David MacDermot.
*
*
* History:
*                JUNE '06 - Created
*                JULY '07 - line 57: Replaced LPTABCTRL this, with LPTABCTRL m_lptc
*                                    and updated the rest of the code and comments
*                                    to reflect the change.
*                JUNE '09 - Refactored and simplified the code.  Improved tabbing.
*
\****************************************************************************/

#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include "TabCtrl.h"
#include <stdafx.h>

/****************************************************************************/
// Internal Globals

static LPTABCTRL m_lptc;

/****************************************************************************/
// Local function prototypes

static VOID TabPageMessageLoop(HWND);

/****************************************************************************
*
*     FUNCTION: FirstTabstop_SetFocus
*
*     PURPOSE:  Ensure focus on first tab stop when entering a tab page.
*               WM_NEXTDLGCTL wparam = 1 shifts focus back to the LAST tabstop
*               In this tabpage. Posting a VK_TAB returns focus to the first tab stop
*               and causes it to be recognized as the first tabstop by the
*               WM_KEYDOWN sniffer in the TabPageMessageLoop().
*
*     RETURNS:  VOID
*
* History:
*                June '09 - Created
*
\****************************************************************************/

VOID FirstTabstop_SetFocus(HWND hwnd)
{
	FORWARD_WM_NEXTDLGCTL(hwnd, 1, FALSE, SendMessage);
	FORWARD_WM_KEYDOWN(GetFocus(), VK_TAB, 0, 0, PostMessage);
}

/****************************************************************************
*
*     FUNCTION: TabControl_GetClientRect
*
*     PURPOSE:  Return a Client rectangle for the tab control under
*               every possible configuration (tabs, buttons vert etc.)
*
*     PARAMS:   HWND hwnd - handle of a tab control
*               RECT *prc - pointer to a rectangle structure 
*
*     RETURNS:  VOID
*
*     COMMENTS: The rectangle structure is populated as follows:
*               prc.left = left, prc.top = top, prc.right = WIDTH, and prc.bottom = HEIGHT 
*
* History:
*                June '06 - Created
*
\****************************************************************************/

VOID TabControl_GetClientRect(HWND hwnd, RECT * prc)
{
	RECT rtab_0;
	LONG lStyle = GetWindowLong(hwnd, GWL_STYLE);

	// Calculate the tab control's display area
	GetWindowRect(hwnd, prc);
	ScreenToClient(GetParent(hwnd), (POINT *) & prc->left);
	ScreenToClient(hwnd, (POINT *) & prc->right);
	TabCtrl_GetItemRect(hwnd, 0, &rtab_0);	//The tab itself

	if ((lStyle & TCS_BOTTOM) && (lStyle & TCS_VERTICAL))	//Tabs to Right
	{
		prc->top = prc->top + 6;	//x coord
		prc->left = prc->left + 4;	//y coord
		prc->bottom = prc->bottom - 12;	// height
		prc->right = prc->right - (12 + rtab_0.right - rtab_0.left);	// width
	}
	else if (lStyle & TCS_VERTICAL)	//Tabs to Left
	{
		prc->top = prc->top + 6;	//x coord
		prc->left = prc->left + (4 + rtab_0.right - rtab_0.left);	//y coord
		prc->bottom = prc->bottom - 12;	// height
		prc->right = prc->right - (12 + rtab_0.right - rtab_0.left);	// width
	}
	else if (lStyle & TCS_BOTTOM)	//Tabs on Bottom
	{
		prc->top = prc->top + 6;	//x coord
		prc->left = prc->left + 4;	//y coord
		prc->bottom = prc->bottom - (16 + rtab_0.bottom - rtab_0.top);	// height
		prc->right = prc->right - 12;	// width
	}
	else	//Tabs on top
	{
		prc->top = prc->top + (6 + rtab_0.bottom - rtab_0.top);	//x coord
		prc->left = prc->left + 4;	//y coord
		prc->bottom = prc->bottom - (16 + rtab_0.bottom - rtab_0.top);	// height
		prc->right = prc->right - 12;	// width
	}
}

/****************************************************************************
*
*     FUNCTION: CenterTabPage
*
*     PURPOSE:  Center the tab page in the tab control's display area.
*
*     PARAMS:   HWND hTab - handle tab control
*               INT iPage - index of tab page
*
*     RETURNS:  BOOL - TRUE if successful
*
*
* History:
*                June '06 - Created
*
\****************************************************************************/

BOOL CenterTabPage(HWND hTab, INT iPage)
{
	RECT rect, rclient;

	// Update m_lptc pointer
	m_lptc = (LPTABCTRL)GetWindowLong(hTab, GWL_USERDATA);

	TabControl_GetClientRect(hTab, &rect);	// left, top, width, height

	// Get the tab page size
	GetClientRect(m_lptc->hTabPages[iPage], &rclient);
	rclient.right = rclient.right - rclient.left;	// width
	rclient.bottom = rclient.bottom - rclient.top;	// height
	rclient.left = rect.left;
	rclient.top = rect.top;

	// Center the tab page, or cut it off at the edge of the tab control(bad)
	if (rclient.right < rect.right)
		rclient.left += (rect.right - rclient.right) / 2;

	if (rclient.bottom < rect.bottom)
		rclient.top += (rect.bottom - rclient.bottom) / 2;

	// Move the child and put it on top
	return SetWindowPos(m_lptc->hTabPages[iPage], HWND_TOP, rclient.left, rclient.top, rclient.right, rclient.bottom, 0);
}

/****************************************************************************
*
*     FUNCTION: StretchTabPage
*
*     PURPOSE:  Stretch the tab page to fit the tab control's display area.
*
*     PARAMS:   HWND hTab - handle tab control
*               INT iPage - index of tab page
*
*     RETURNS:  BOOL - TRUE if successful
*
*
* History:
*                June '06 - Created
*
\****************************************************************************/

BOOL StretchTabPage(HWND hTab, INT iPage)
{
	RECT rect;

	// Update m_lptc pointer
	m_lptc = (LPTABCTRL)GetWindowLong(hTab, GWL_USERDATA);

	TabControl_GetClientRect(hTab, &rect);	// left, top, width, height

	// Move the child and put it on top
	return SetWindowPos(m_lptc->hTabPages[iPage], HWND_TOP, rect.left, rect.top, rect.right, rect.bottom, 0);
}

/****************************************************************************
*
*     FUNCTION: TabCtrl_OnKeyDown
*
*     PURPOSE:  Handle key presses in the tab control (but not the tab pages).
*
*     PARAMS:   LPARAM lParam - lParam furnished by parent proc's WM_NOTIFY handler
*
*     RETURNS:  VOID
*
* History:
*                June '06 - Created
*
\****************************************************************************/

VOID TabCtrl_OnKeyDown(LPARAM lParam)
{
	TC_KEYDOWN *tk = (TC_KEYDOWN *)lParam;
	int itemCount = TabCtrl_GetItemCount(tk->hdr.hwndFrom);
	int currentSel = TabCtrl_GetCurSel(tk->hdr.hwndFrom);

	if (itemCount <= 1)
		return;	// Ignore if only one TabPage

	BOOL verticalTabs = (BOOL)(GetWindowLong(m_lptc->hTab, GWL_STYLE) & TCS_VERTICAL);

	if (verticalTabs)
	{
		switch (tk->wVKey)
		{
			case VK_PRIOR:	//select the previous page
				if (0 == currentSel)
					return;
				TabCtrl_SetCurSel(tk->hdr.hwndFrom, currentSel - 1);
				TabCtrl_SetCurFocus(tk->hdr.hwndFrom, currentSel - 1);
				break;

			case VK_UP:	//select the previous page
				if (0 == currentSel)
					return;
				TabCtrl_SetCurSel(tk->hdr.hwndFrom, currentSel - 1);
				TabCtrl_SetCurFocus(tk->hdr.hwndFrom, currentSel);
				break;

			case VK_NEXT:	//select the next page
				TabCtrl_SetCurSel(tk->hdr.hwndFrom, currentSel + 1);
				TabCtrl_SetCurFocus(tk->hdr.hwndFrom, currentSel + 1);
				break;

			case VK_DOWN:	//select the next page
				TabCtrl_SetCurSel(tk->hdr.hwndFrom, currentSel + 1);
				TabCtrl_SetCurFocus(tk->hdr.hwndFrom, currentSel);
				break;

			case VK_LEFT:	//navagate within selected child tab page
			case VK_RIGHT:
				SetFocus(m_lptc->hTabPages[currentSel]);
				FirstTabstop_SetFocus(m_lptc->hTabPages[currentSel]);
				TabPageMessageLoop(m_lptc->hTabPages[currentSel]);
				break;

			default:
				return;
		}
	}	// if(verticalTabs)

	else	// horizontal Tabs
	{
		switch (tk->wVKey)
		{
			case VK_NEXT:	//select the previous page
				if (0 == currentSel)
					return;
				TabCtrl_SetCurSel(tk->hdr.hwndFrom, currentSel - 1);
				TabCtrl_SetCurFocus(tk->hdr.hwndFrom, currentSel - 1);
				break;

			case VK_LEFT:	//select the previous page
				if (0 == currentSel)
					return;
				TabCtrl_SetCurSel(tk->hdr.hwndFrom, currentSel - 1);
				TabCtrl_SetCurFocus(tk->hdr.hwndFrom, currentSel);
				break;

			case VK_PRIOR:	//select the next page
				TabCtrl_SetCurSel(tk->hdr.hwndFrom, currentSel + 1);
				TabCtrl_SetCurFocus(tk->hdr.hwndFrom, currentSel + 1);
				break;

			case VK_RIGHT:	//select the next page
				TabCtrl_SetCurSel(tk->hdr.hwndFrom, currentSel + 1);
				TabCtrl_SetCurFocus(tk->hdr.hwndFrom, currentSel);
				break;

			case VK_UP:	//navagate within selected child tab page
			case VK_DOWN:
				SetFocus(m_lptc->hTabPages[currentSel]);
				FirstTabstop_SetFocus(m_lptc->hTabPages[currentSel]);
				TabPageMessageLoop(m_lptc->hTabPages[currentSel]);
				break;

			default:
				return;
		}
	}	//else // horizontal Tabs
}

/****************************************************************************
*
*     FUNCTION: TabPage_OnCommand
*
*     PURPOSE:  Handle the tab page's virtual keys while
*               forwarding the rest of the commands to ParentProc.
*
*     PARAMS:   HWND hwnd - handle to the tab page (dialog)
*               INT id - Control id
*               HWND hwndCtl - Controls handle
*               UINT codeNotify - Notification code 
*
*     RETURNS:  VOID
*
* History:
*                June '06 - Created
*
\****************************************************************************/

VOID TabPage_OnCommand(HWND hwnd, INT id, HWND hwndCtl, UINT codeNotify)
{
	//Forward all commands to the parent proc.
	//Note: We don't use SendMessage because on the receiving end,
	// that is: _OnCommand(HWND hwnd, INT id, HWND hwndCtl, UINT codeNotify),
	// hwnd would = addressee and not the parent of the control.  Intuition
	// favors the notion that hwnd is the parent of hwndCtl.
	FORWARD_WM_COMMAND(hwnd, id, hwndCtl, codeNotify, m_lptc->ParentProc);

	// If this WM_COMMAND message is a notification to parent window
	// ie: EN_SETFOCUS being sent when an edit control is initialized
	// do not engage the Message Loop or send any messages.
	if (codeNotify != 0)
		return;

	// Mouse clicks on a control should engage the Message Loop
	SetFocus(hwndCtl);
	FirstTabstop_SetFocus(hwnd);
	TabPageMessageLoop(hwnd);
}

/****************************************************************************
*
*     FUNCTION: TabPage_OnLButtonDown
*
*     PURPOSE:  Handle WM_LBUTTONDOWN (tab page dialog)
*
*     PARAMS:   HWND hwnd - handle to the tab page (dialog)
*               BOOL fDoubleClick - Was this a double click?
*               INT x - Coordinate
*               INT y - Coordinate
*               UINT keyFlags - Flags 
*
*     RETURNS:  VOID
*
* History:
*                June '06 - Created
*
\****************************************************************************/

VOID TabPage_OnLButtonDown(HWND hwnd, BOOL fDoubleClick, INT x, INT y, UINT keyFlags)
{
	// If Mouse click in tab page but not on control simulate keypress access
	//  so that everything is handled in the same consistant way
	BOOL verticalTabs = GetWindowLong(m_lptc->hTab, GWL_STYLE) & TCS_VERTICAL;

	if (verticalTabs)
	{
		NMTCKEYDOWN nm = { m_lptc->hTab, GetDlgCtrlID(m_lptc->hTab), TCN_KEYDOWN, VK_LEFT, 0 };
		FORWARD_WM_NOTIFY(nm.hdr.hwndFrom,nm.hdr.idFrom, &nm, SendMessage);
	}
	else
	{
		NMTCKEYDOWN nm = { m_lptc->hTab, GetDlgCtrlID(m_lptc->hTab), TCN_KEYDOWN, VK_DOWN, 0 };
		FORWARD_WM_NOTIFY(nm.hdr.hwndFrom,nm.hdr.idFrom, &nm, SendMessage);
	}
}

/****************************************************************************
*
*     FUNCTION: TabPage_OnSize
*
*     PURPOSE:  Handle WM_SIZE (tab page dialog)
*
*     PARAMS:   HWND hwnd - handle to the tab page (dialog)
*               UINT state - resizing flag
*               INT cx - Width
*               INT cy - Height
*
*     RETURNS:  VOID
*
* History:
*                June '06 - Created
*
\****************************************************************************/

VOID TabPage_OnSize(HWND hwnd, UINT state, INT cx, INT cy)
{
	// Dummy function when no external on size function is
	// desired.
}

/****************************************************************************
*
*     FUNCTION: TabPage_OnInitDialog
*
*     PURPOSE:  Handle WM_INITDIALOG (tab page dialog)
*
*     PARAMS:   HWND hwnd - handle to the tab page (dialog)
*               HWND hwndFocus - handle of control to receive focus
*               LPARAM lParam - initialization parameter
*
*     RETURNS:  BOOL - TRUE if handled
*
* History:
*                June '06 - Created
*
\****************************************************************************/

BOOL TabPage_OnInitDialog(HWND hwnd, HWND hwndFocus, LPARAM lParam)
{
	// We handle this message so that it is not sent to the main dlg proc
	// each time a tab page is instanciated.
	return DefWindowProc(hwnd, WM_INITDIALOG, (WPARAM)hwndFocus, lParam);
}

/****************************************************************************
*
*     FUNCTION: TabPage_DlgProc
*
*     PURPOSE:  Window Procedure for the tab pages (dialogs).
*
*     PARAMS:   HWND   hwnd		- This window
*               UINT   msg		- Which message?
*               WPARAM wParam	- message parameter
*               LPARAM lParam	- message parameter
*
*     RETURNS:  BOOL CALLBACK - depends on message
*
* History:
*                June '06 - Created
*
\****************************************************************************/

BOOL CALLBACK TabPage_DlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
	// Update m_lptc pointer
	static HWND hTab;
	hTab = (HWND)GetWindowLong(hwndDlg, GWL_USERDATA);
	if (NULL != hTab)
		m_lptc = (LPTABCTRL)GetWindowLong(hTab, GWL_USERDATA);

	switch (msg)
	{
		HANDLE_DLGMSG(hwndDlg, WM_INITDIALOG, TabPage_OnInitDialog);
		HANDLE_DLGMSG(hwndDlg, WM_SIZE, m_lptc->TabPage_OnSize);
		HANDLE_DLGMSG(hwndDlg, WM_COMMAND, TabPage_OnCommand);
		HANDLE_DLGMSG(hwndDlg, WM_LBUTTONDOWN, TabPage_OnLButtonDown);
		//// TODO: Add TabPage dialog message crackers here...

		default:
		{
			m_lptc->ParentProc(hwndDlg, msg, wParam, lParam);
			return FALSE;
		}
	}
}

/****************************************************************************
*
*     FUNCTION: TabPageMessageLoop
*
*     PURPOSE:  Monitor and respond to user keyboard input and system messages.
*
*     PARAMS:   HWND hwnd - handle to the currently visible tab page
*
*     RETURNS:  VOID
*
*     COMMENTS: Send PostQuitMessage(0); from any cancel or exit event.	
*               Failure to do so will leave the process running even after
*               application exit.
*
* History:
*                June '06 - Created
*
\****************************************************************************/

static VOID TabPageMessageLoop(HWND hwnd)
{
	MSG msg;
	int status;
	BOOL handled = FALSE;
	BOOL fFirstStop = FALSE;
	HWND hFirstStop;

	while ((status = GetMessage(&msg, NULL, 0, 0)))
	{
		if (-1 == status)	// Exception
		{
			return;
		}
		else
		{
			//This message is explicitly posted from TabCtrl_OnSelChanged() to
			// indicate the closing of this page.  Stop the Loop.
			if (WM_SHOWWINDOW == msg.message && FALSE == msg.wParam)
				return;

			//IsDialogMessage() dispatches WM_KEYDOWN to the tab page's child controls
			// so we'll sniff them out before they are translated and dispatched.
			if (WM_KEYDOWN == msg.message && VK_TAB == msg.wParam)
			{
				//Tab each tabstop in a tab page once and then return to to
				// the tabCtl selected tab
				if (!fFirstStop)
				{
					fFirstStop = TRUE;
					hFirstStop = msg.hwnd;
				}
				else if (hFirstStop == msg.hwnd)
				{
					// Tab off the tab page
					HWND hTab = (HWND)GetWindowLong(GetParent(msg.hwnd), GWL_USERDATA);
					if (NULL == hTab)
						hTab = m_lptc->hTab;
					SetFocus(hTab);
					return;
				}
			}
			// Perform default dialog message processing using IsDialogM. . .
			handled = IsDialogMessage(hwnd, &msg);

			// Non dialog message handled in the standard way.
			if (!handled)
			{
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}
		}
	}
	// If we get here window is closing
	PostQuitMessage(0);
	return;
}

/****************************************************************************
*
*     FUNCTION: TabCtrl_OnSelChanged
*
*     PURPOSE:  A tab has been pressed, Handle TCN_SELCHANGE notification.
*
*     PARAMS:   VOID
*
*     RETURNS:  BOOL - TRUE if handled
*
* History:
*                June '06 - Created
*
\****************************************************************************/

BOOL TabCtrl_OnSelChanged(VOID)
{
	int iSel = TabCtrl_GetCurSel(m_lptc->hTab);

	//Hide the current child dialog box, if any.
	ShowWindow(m_lptc->hVisiblePage, FALSE);

	//ShowWindow() does not seem to post the WM_SHOWWINDOW message
	// to the tab page.  Since I use the hiding of the window as an
	// indication to stop the message loop I post it explicitly.
	//PostMessage() is necessary in the event that the Loop was started
	// in response to a mouse click.
	FORWARD_WM_SHOWWINDOW(m_lptc->hVisiblePage,FALSE,0,PostMessage);

	//Show the new child dialog box.
	ShowWindow(m_lptc->hTabPages[iSel], TRUE);

	// Save the current child
	m_lptc->hVisiblePage = m_lptc->hTabPages[iSel];

	return TRUE;
}

/****************************************************************************
*
*     FUNCTION: Notify
*
*     PURPOSE:  Handle WM_NOTIFY messages.
*
*     PARAMS:   VOID
*
*     RETURNS:  BOOL - TRUE if handled
*
*     COMMENTS: This function is made external to module via function pointer
*
* History:
*                June '07 - Created
*
\****************************************************************************/
BOOL Notify(LPNMHDR pnm)
{
	// Update m_lptc pointer
	m_lptc = (LPTABCTRL)GetWindowLong(pnm->hwndFrom, GWL_USERDATA);

	switch (pnm->code)
	{
		case TCN_KEYDOWN:
			TabCtrl_OnKeyDown((LPARAM)pnm);

		// fall through to call TabCtrl_OnSelChanged() on each keydown
		case TCN_SELCHANGE:
			return TabCtrl_OnSelChanged();
	}
	return FALSE;
}

/****************************************************************************
*
*     FUNCTION: TabControl_Destroy
*
*     PURPOSE:  Destroy the tab page dialogs and
*               free the list of pointers to the dialogs.
*
*     PARAMS:   LPTABCTRL tc - Pointer to a TABCTRL structure
*
*     RETURNS:  VOID
*
*
* History:
*                June '06 - Created
*
\****************************************************************************/

VOID TabControl_Destroy(LPTABCTRL tc)
{
	for (int i = 0; i < tc->tabPageCount; i++)
		DestroyWindow(tc->hTabPages[i]);

	free(tc->hTabPages);
}

/****************************************************************************
*
*     FUNCTION: New_TabControl
*
*     PURPOSE:  Initialize a TABCTRL struct, create tab page dialogs
*               and start the message loop.
*
*     PARAMS:   LPTABCTRL tc - Pointer to a TABCTRL structure
*               HWND hTab - Handle to the parent tab control
*               LPSTR *tabNames - Pointer to array of tab names
*               LPSTR *dlgNames - Pointer to array of MAKEINTRESOURCE() dialog ids
*               BOOL CALLBACK(*ParentProc)(HWND, UINT, WPARAM, LPARAM) - function pointer
*               VOID (*OnSize)(HWND, UINT, int, int) - Optional function pointer
*               BOOL fStretch) - stretch dialog flag
*
*     RETURNS:  VOID
*
*
* History:
*                June '06 - Created
*
\****************************************************************************/

VOID New_TabControl(LPTABCTRL lptc,
					HWND hTab,
					LPSTR *tabNames,
					LPSTR *dlgNames,
					BOOL(CALLBACK *ParentProc) (HWND, UINT, WPARAM, LPARAM),
					VOID(*OnSize) (HWND, UINT, INT, INT), BOOL fStretch)
{
	static TCITEM tie;
	m_lptc = lptc;

	//Link struct m_lptc pointer to hTab
	SetWindowLong(hTab, GWL_USERDATA, (LONG)m_lptc);

	m_lptc->hTab = hTab;
	m_lptc->tabNames = tabNames;
	m_lptc->blStretchTabs = fStretch;

	// Point to external functions
	m_lptc->ParentProc = ParentProc;

	if (NULL != OnSize)	//external size function
		m_lptc->TabPage_OnSize = OnSize;
	else	//internal dummy size function
		m_lptc->TabPage_OnSize = TabPage_OnSize;

	// Point to internal public functions
	m_lptc->Notify = &Notify;
	m_lptc->StretchTabPage = &StretchTabPage;
	m_lptc->CenterTabPage = &CenterTabPage;

	// Determine number of tab pages to insert based on DlgNames
	m_lptc->tabPageCount = 0;
	LPSTR *ptr = dlgNames;
	while (*ptr++)
		m_lptc->tabPageCount++;

	//create array based on number of pages
	m_lptc->hTabPages = (HWND*)malloc(m_lptc->tabPageCount * sizeof(HWND*));

	// Add a tab for each name in tabnames (list ends with 0)
	tie.mask = TCIF_TEXT | TCIF_IMAGE;
	tie.iImage = -1;
	for (int i = 0; i < m_lptc->tabPageCount; i++)
	{
		tie.pszText = m_lptc->tabNames[i];
		TabCtrl_InsertItem(m_lptc->hTab, i, &tie);

		// Add page to each tab
		m_lptc->hTabPages[i] = CreateDialog(GetModuleHandle(NULL), dlgNames[i], GetParent(m_lptc->hTab), (DLGPROC)TabPage_DlgProc);

		// Since the hTab is not the paren link it to this page 
		SetWindowLong(m_lptc->hTabPages[i], GWL_USERDATA, (LONG)hTab);

		// Set initial tab page position
		if (m_lptc->blStretchTabs)
			m_lptc->StretchTabPage(m_lptc->hTab, i);
		else
			m_lptc->CenterTabPage(m_lptc->hTab, i);
	}
	// Show first tab
	ShowWindow(m_lptc->hTabPages[0], SW_SHOW);

	// Save the current child
	m_lptc->hVisiblePage = m_lptc->hTabPages[0];
}
