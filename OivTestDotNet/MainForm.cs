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
        public static extern int OIV_Execute(int command, int requestSize, IntPtr request,int responseSize,IntPtr response);

        [StructLayout(LayoutKind.Sequential, Pack= 1)]
        struct CmdDataLoadFile
        {
            public int /*std::size_t*/ FileNamelength;
            public IntPtr filePath;
        };

        [StructLayout(LayoutKind.Sequential, Pack = 1)]
        struct CmdDataInit
        {

            public IntPtr /*std::size_t*/ parentHandle;
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

        struct NullStruct
        {

        };

        NullStruct? nullStruct;


        void ExecuteCommand<T,U>(int command,ref T? request, ref U? response) where T : struct where U : struct
    {
            IntPtr requestAddr = IntPtr.Zero;
            IntPtr responseAddr = IntPtr.Zero;
            GCHandle requestHandle = default(GCHandle);
            GCHandle responseHandle = default(GCHandle);
            int requestSize = 0;
            int responseSize = 0;
            byte[] responseByteArray = null;

            if (request != null)
            {
                requestHandle = GCHandle.Alloc(request, GCHandleType.Pinned);
                requestAddr = requestHandle.AddrOfPinnedObject();
                requestSize = Marshal.SizeOf(request);
            }

            if (response != null)
            {
                responseSize = Marshal.SizeOf(response);
                responseByteArray = new byte[responseSize];

                responseHandle = GCHandle.Alloc(responseByteArray, GCHandleType.Pinned);
                responseAddr = responseHandle.AddrOfPinnedObject();
            }

            if (OIV_Execute(command, requestSize, requestAddr, responseSize, responseAddr)  != 0)
                throw new Exception("Api function failed");

            if (requestHandle.IsAllocated)
                requestHandle.Free();

            if (responseHandle.IsAllocated)
                responseHandle.Free();


            if (responseSize > 0)
            {
                response = ByteArrayToStructure<U>(responseByteArray);
            }
    }

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
            CmdDataInit init;
            init.parentHandle = panel1.Handle;
            CmdDataInit? initNullable = new CmdDataInit?(init);
            ExecuteCommand(CE_Init, ref initNullable, ref nullStruct);
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

            CmdDataLoadFile? loadFileNullable = new CmdDataLoadFile?(loadFile);
            ExecuteCommand(CE_LoadImage, ref loadFileNullable, ref nullStruct);
        }

        private void Form1_SizeChanged(object sender, EventArgs e)
        {
            const int CE_Refresh = 7;
            ExecuteCommand(CE_Refresh, ref nullStruct, ref nullStruct);
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
            const int CE_GetFileInformation = 8;
            QryFileInformation fileInfo = new QryFileInformation();
            QryFileInformation? fileInfoNullable = new QryFileInformation?(fileInfo);

            ExecuteCommand(CE_GetFileInformation, ref nullStruct, ref fileInfoNullable);
            fileInfo = fileInfoNullable.Value;





            MessageBox.Show($"Width:{fileInfo.width}\n" +
                            $"Height: { fileInfo.height}\n" +
                            $"BitsPerPixel: { fileInfo.bitsPerPixel} \n" +
                            $"ImageDataSize: { fileInfo.imageDataSize}\n" + 
                            $"Transparency: { fileInfo.hasTransparency}\n"+ 
                            $"NumberOfMipMaps: { fileInfo.numMipMaps}\n"
            );

        }
        
    }
}
