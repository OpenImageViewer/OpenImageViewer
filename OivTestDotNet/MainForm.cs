using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Linq;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Forms;

namespace OivTestDotNet
{
    public partial class MainForm : Form
    {

        [DllImport("oiv.dll",CallingConvention = CallingConvention.Cdecl)]
        public static extern int OIV_Execute(int command, int commandSize, IntPtr commandData);
        [DllImport("oiv.dll", CallingConvention = CallingConvention.Cdecl)]
        public static extern int OIV_Query(int command, IntPtr commandData, int commandSize, IntPtr output_data, int output_size);

        [StructLayout(LayoutKind.Sequential, Pack= 1)]
        struct CmdDataLoadFile
        {
            public int /*size_t*/ FileNamelength;
            public IntPtr filePath;
        };

        [StructLayout(LayoutKind.Sequential, Pack = 1)]
        struct CmdDataInit
        {

            public IntPtr /*size_t*/ parentHandle;
        };
        [StructLayout(LayoutKind.Sequential, Pack = 1)]
        struct CmdDataClientMetrics
        {
            public int width;
            public int height;
        };
        [StructLayout(LayoutKind.Sequential, Pack = 1)]
        struct QryFileInformation
        {
            public int width;
            public int height;
            public int bitsPerPixel;
            public int numMipMaps;
            public int numChannels;
            public int imageDataSize;
            public int rowPitchInBytes;
            public int hasTransparency;
        };

        public MainForm()
        {
            InitializeComponent();
        }

        private void Form1_Load(object sender, EventArgs e)
        {
       
        }

        private void button1_Click(object sender, EventArgs e)
        {
            // Init
            const int CE_Init = 1;

            CmdDataInit init = new CmdDataInit();
            init.parentHandle =  panel1.Handle;
            
            GCHandle handle = GCHandle.Alloc(init, GCHandleType.Pinned);

            if (OIV_Execute(CE_Init, Marshal.SizeOf(init), handle.AddrOfPinnedObject()) != 0)
                throw new Exception("Unable to initialize Image rendering engine.");

            handle.Free();
           
        }

        private void button2_Click(object sender, EventArgs e)
        {
            // Load image
            const int CE_LoadImage = 3;
            string filePath = @"d:\PNG_transparency_demonstration_1.png";
            GCHandle handle = GCHandle.Alloc(filePath, GCHandleType.Pinned);

            CmdDataLoadFile loadFile = new CmdDataLoadFile();
            //loadFile.filePath = L"d:/1.png";
            loadFile.filePath = handle.AddrOfPinnedObject();
            loadFile.FileNamelength = filePath.Length;

            GCHandle structHandle = GCHandle.Alloc(loadFile, GCHandleType.Pinned);


            if (OIV_Execute(CE_LoadImage, Marshal.SizeOf(loadFile), structHandle.AddrOfPinnedObject()) != 0)
                throw new Exception("Unable to Load image.");

            structHandle.Free();
            handle.Free();
        }

        private void Form1_SizeChanged(object sender, EventArgs e)
        {
            const int CE_Refresh = 7;
            if (OIV_Execute(CE_Refresh, 0, IntPtr.Zero) != 0)
                throw new Exception("Error while trying to refresh image rendering engine.");
        }

        T ByteArrayToStructure<T>(byte[] bytes) where T : struct
        {
            GCHandle handle = GCHandle.Alloc(bytes, GCHandleType.Pinned);
            T stuff = (T)Marshal.PtrToStructure(handle.AddrOfPinnedObject(), typeof(T));
            handle.Free();
            return stuff;
        }

        private void button3_Click(object sender, EventArgs e)
        {
            int dataSize = Marshal.SizeOf<QryFileInformation>();
            byte[] byteArray = new byte[dataSize];
            GCHandle structHandle = GCHandle.Alloc(byteArray, GCHandleType.Pinned);

            const int CQ_GetFileInformation = 1;

            if (OIV_Query(CQ_GetFileInformation, IntPtr.Zero, 0, structHandle.AddrOfPinnedObject(), dataSize) != 0)
                throw new Exception("Could not get file information.");

            //Marshal.PtrToStructure(ptr, output_fileInfo);
            structHandle.Free();
            QryFileInformation output_fileInfo = ByteArrayToStructure<QryFileInformation>(byteArray);

            MessageBox.Show($"Width:{output_fileInfo.width}\n" +
                            $"Height: { output_fileInfo.height}\n" +
                            $"BitsPerPixel: { output_fileInfo.bitsPerPixel} \n" +
                            $"ImageDataSize: { output_fileInfo.imageDataSize}\n" + 
                            $"Transparency: { output_fileInfo.hasTransparency}\n"+ 
                            $"NumberOfMipMaps: { output_fileInfo.numMipMaps}\n"
            );

        }
        
    }
}
