#include "DragAndDrop.h"
#include "Win32Window.h"

namespace OIV
{
    namespace Win32
    {
        DragAndDropTarget::DragAndDropTarget(Win32Window& parentWindow):
            m_cRef(1)
            , fParentWindow(parentWindow)
        {
            if (SUCCEEDED(OleInitialize(nullptr)))
            {
                if (SUCCEEDED(RegisterDragDrop(fParentWindow.GetHandle(), this)))
                {
                    //Logger::Log("Drag and drop target initialized");
                }
            }


        }

        DragAndDropTarget::~DragAndDropTarget()
        {

        }

        void DragAndDropTarget::Detach()
        {
            RevokeDragDrop(fParentWindow.GetHandle());
        }

        HRESULT DragAndDropTarget::QueryInterface(const IID& riid, void** ppv)
        {
            if (riid == IID_IUnknown || riid == IID_IDropTarget)
            {
                *ppv = static_cast<IUnknown*>(this);
                AddRef();
                return S_OK;
            }
            *ppv = nullptr;
            return E_NOINTERFACE;
        }

        ULONG DragAndDropTarget::AddRef()
        {
            return InterlockedIncrement(&m_cRef);
        }

        ULONG DragAndDropTarget::Release()
        {
            LONG cRef = InterlockedDecrement(&m_cRef);
            if (cRef == 0) delete this;
            return cRef;
        }

        HRESULT DragAndDropTarget::DragEnter([[maybe_unused]] IDataObject* pdto, [[maybe_unused]] DWORD grfKeyState, [[maybe_unused]] POINTL ptl, DWORD* pdwEffect)
        {
            *pdwEffect &= DROPEFFECT_COPY;
            return S_OK;
        }

        HRESULT DragAndDropTarget::DragOver([[maybe_unused]] DWORD grfKeyState, [[maybe_unused]] POINTL ptl, DWORD* pdwEffect)
        {
            *pdwEffect &= DROPEFFECT_COPY;
            return S_OK;
        }

        HRESULT DragAndDropTarget::DragLeave()
        {
            return S_OK;
        }

        HRESULT DragAndDropTarget::Drop(IDataObject* pdto, [[maybe_unused]]  DWORD grfKeyState, [[maybe_unused]]  POINTL ptl, DWORD* pdwEffect)
        {
            OpenFilesFromDataObject(pdto);
            *pdwEffect &= DROPEFFECT_COPY;
            return S_OK;
        }

        void DragAndDropTarget::OpenFilesFromDataObject(IDataObject* pdto)
        {
            FORMATETC fmte = { CF_HDROP, nullptr, DVASPECT_CONTENT,
                -1, TYMED_HGLOBAL };
            STGMEDIUM stgm;
            if (SUCCEEDED(pdto->GetData(&fmte, &stgm)))
            {
                HDROP hdrop = reinterpret_cast<HDROP>(stgm.hGlobal);

                UINT cFiles = DragQueryFile(hdrop, 0xFFFFFFFF, nullptr, 0);
                for (UINT i = 0; i < cFiles; i++)
                {
                    TCHAR szFile[MAX_PATH];
                    UINT cch = DragQueryFile(hdrop, i, szFile, MAX_PATH);
                    if (cch > 0 && cch < MAX_PATH)
                    {
                        EventDdragDropFile evnt;
                        //DragDropFile evnt;
                        evnt.fileName = szFile;
                        fParentWindow.RaiseEvent(evnt);
                    }
                }
                ReleaseStgMedium(&stgm);
            }
        }
    }
}
