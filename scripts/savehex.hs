-- Take Octo output like [0x00, 0xFF] to stdin and output it as binary to stdout.
import qualified Data.ByteString as B
main = getLine >>= B.putStr . B.pack . read
