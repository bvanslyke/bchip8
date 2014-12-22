
echo "$1" | runhaskell scripts/savehex.hs > tmp.c8 && bin/bchip tmp.c8
