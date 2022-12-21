using System;
using System.Collections.Generic;
using System.Linq;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading.Tasks;

namespace ConsoleApp1
{
    // enum AVCodecID at https://ffmpeg.org/doxygen/3.2/group__lavc__core.html#gaadca229ad2c20e060a14fec08a5cc7ce
    public enum FFmpegVideoCodecId
    {
        MJPEG = 7,
        H264 = 27,
        HEVC = 173,
    }

    [Flags]
    public enum FFmpegScalingQuality
    {
        FastBilinear = 1,
        Bilinear = 2,
        Bicubic = 4,
        Point = 0x10,
        Area = 0x20,
    }

    public enum FFmpegPixelFormat
    {
        None = -1,
        BGR24 = 3,
        GRAY8 = 8,
        YUVJ420P = 12,
        BGRA = 28
    }

    public enum EHwAccel
    {
        NO_ACCEL = 0,
        QSV = 1
    }

    internal static class FFmpegVideoPInvoke
    {
        private const string LibraryName = "libffmpeghelper.dll";

        [DllImport(LibraryName, EntryPoint = "create_video_decoder", CallingConvention = CallingConvention.Cdecl)]
        public static extern int CreateVideoDecoder(FFmpegVideoCodecId videoCodecId, EHwAccel hwAccel, out IntPtr handle);

        [DllImport(LibraryName, EntryPoint = "remove_video_decoder", CallingConvention = CallingConvention.Cdecl)]
        public static extern void RemoveVideoDecoder(IntPtr handle);

        [DllImport(LibraryName, EntryPoint = "set_video_decoder_extradata",
            CallingConvention = CallingConvention.Cdecl)]
        public static extern int SetVideoDecoderExtraData(IntPtr handle, IntPtr extradata, int extradataLength);

        [DllImport(LibraryName, EntryPoint = "decode_video_frame", CallingConvention = CallingConvention.Cdecl)]
        public static extern int DecodeFrame(IntPtr handle, IntPtr rawBuffer, int rawBufferLength, out int frameWidth,
            out int frameHeight, out FFmpegPixelFormat framePixelFormat);

        [DllImport(LibraryName, EntryPoint = "custom_alloc", CallingConvention = CallingConvention.Cdecl)]
        public static unsafe extern void* Malloc(int buffer_size);

        [DllImport(LibraryName, EntryPoint = "free_buff", CallingConvention = CallingConvention.Cdecl)]
        public static extern void Free(IntPtr buffer);

        [DllImport(LibraryName, EntryPoint = "custom_free", CallingConvention = CallingConvention.Cdecl)]
        public static extern void CustomFree(IntPtr buff);

        [DllImport(LibraryName, EntryPoint = "scale_decoded_video_frame", CallingConvention = CallingConvention.Cdecl)]
        public static extern int ScaleDecodedVideoFrame(IntPtr handle, IntPtr scalerHandle, IntPtr scaledBuffer,
            int scaledBufferStride);

        [DllImport(LibraryName, EntryPoint = "create_video_scaler", CallingConvention = CallingConvention.Cdecl)]
        public static extern int CreateVideoScaler(int sourceLeft, int sourceTop, int sourceWidth, int sourceHeight,
            FFmpegPixelFormat sourcePixelFormat,
            int scaledWidth, int scaledHeight, FFmpegPixelFormat scaledPixelFormat, FFmpegScalingQuality qualityFlags,
            out IntPtr handle);

        [DllImport(LibraryName, EntryPoint = "remove_video_scaler", CallingConvention = CallingConvention.Cdecl)]
        public static extern void RemoveVideoScaler(IntPtr handle);
    }
}
