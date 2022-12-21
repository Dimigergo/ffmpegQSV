using ConsoleApp1;

try
{
    IntPtr decoderPtr;
    int resultCode = FFmpegVideoPInvoke.CreateVideoDecoder(FFmpegVideoCodecId.H264, EHwAccel.QSV, out decoderPtr);

    if (resultCode == 0)
    {
        byte[] frameArr = File.ReadAllBytes("frame.dat");
        byte[] spsByteArr = File.ReadAllBytes("sps.dat");
        byte[] ppsByteArr = File.ReadAllBytes("pps.dat");

        byte[] extraData = new byte[spsByteArr.Length + ppsByteArr.Length];
        Buffer.BlockCopy(spsByteArr, 0, extraData, 0, spsByteArr.Length);
        Buffer.BlockCopy(ppsByteArr, 0, extraData, spsByteArr.Length, ppsByteArr.Length);

        unsafe
        {
            fixed (byte* rawBufferPtr = &frameArr[0])
            {
                fixed (byte* initDataPtr = &extraData[0])
                {
                    resultCode = FFmpegVideoPInvoke.SetVideoDecoderExtraData(decoderPtr, (IntPtr)initDataPtr, extraData.Length);
                    if (resultCode == 0)
                    {
                        resultCode = FFmpegVideoPInvoke.DecodeFrame(decoderPtr, (IntPtr)rawBufferPtr, frameArr.Length,
                            out int width, out int height, out FFmpegPixelFormat pixelFormat);

                        if (resultCode == 0)
                            Console.WriteLine($"decoded width/height: {width}/{height}");
                        else
                            Console.WriteLine($"decoded error: {resultCode}");
                    }
                }
            }
        }
    }

    Console.ReadLine();
}
catch (Exception ex)
{
    Console.WriteLine(ex);
}