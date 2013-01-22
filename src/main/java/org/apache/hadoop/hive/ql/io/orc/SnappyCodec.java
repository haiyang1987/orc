package org.apache.hadoop.hive.ql.io.orc;

import org.iq80.snappy.Snappy;

import java.io.IOException;
import java.nio.ByteBuffer;

class SnappyCodec implements CompressionCodec {

  @Override
  public boolean compress(ByteBuffer in, ByteBuffer out,
                          ByteBuffer overflow) throws IOException {
    int inBytes = in.remaining();
    // i should work on a patch for Snappy to support an overflow buffer
    // to prevent the extra buffer copy
    byte[] compressed = new byte[Snappy.maxCompressedLength(inBytes)];
    int outBytes =
        Snappy.compress(in.array(), in.arrayOffset() + in.position(), inBytes,
            compressed, 0);
    if (outBytes < inBytes) {
      int remaining = out.remaining();
      if (remaining >= outBytes) {
        System.arraycopy(compressed, 0, out.array(), out.arrayOffset() +
            out.position(), outBytes);
        out.position(out.position() + outBytes);
      } else {
        System.arraycopy(compressed, 0, out.array(), out.arrayOffset() +
            out.position(), remaining);
        out.position(out.limit());
        System.arraycopy(compressed, remaining, overflow.array(),
            overflow.arrayOffset(), outBytes - remaining);
        overflow.position(outBytes - remaining);
      }
      return true;
    } else {
      return false;
    }
  }

  @Override
  public void decompress(ByteBuffer in, ByteBuffer out) throws IOException {
    int inOffset = in.position();
    int uncompressLen =
        Snappy.uncompress(in.array(), in.arrayOffset() + inOffset,
        in.limit() - inOffset, out.array(), out.arrayOffset() + out.position());
    out.position(uncompressLen + out.position());
    out.flip();
  }
}
