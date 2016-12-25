#pragma once
#include <shlobj.h>
#include "Event.h"

namespace OIV
{
    namespace Win32
    {
        class DragAndDropTarget : public IDropTarget
        {
#pragma region IUnkown
        private:
            LONG m_cRef;
            Win32WIndow& fParentWindow;
        public:

            DragAndDropTarget(Win32WIndow& parentWindow);
            virtual ~DragAndDropTarget();
            void Detach();
            // *** IUnknown ***
            STDMETHODIMP QueryInterface(REFIID riid, void** ppv);

            STDMETHODIMP_(ULONG) AddRef();

            STDMETHODIMP_(ULONG) Release();

#pragma endregion
#pragma region  IDropTarget
            // *** IDropTarget ***
            STDMETHODIMP DragEnter(IDataObject* pdto,
                                   DWORD grfKeyState, POINTL ptl, DWORD* pdwEffect);

            STDMETHODIMP DragOver(DWORD grfKeyState,
                                  POINTL ptl, DWORD* pdwEffect);

            STDMETHODIMP DragLeave();

            STDMETHODIMP Drop(IDataObject* pdto, DWORD grfKeyState,
                              POINTL ptl, DWORD* pdwEffect);
#pragma endregion


#pragma region ParseAndDispatch
        public:
            typedef std::function< bool(const EventDragDrop&) > DragDropCallback;
        private:
            typedef std::vector <DragDropCallback> DragDropCallbackCollection;
            DragDropCallbackCollection fDragDropListeners;

            void OpenFilesFromDataObject(IDataObject* pdto);
#pragma endregion 
        };

       
    } // namespace Win32
} //namespace OI

