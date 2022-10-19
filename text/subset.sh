# ZH=~/Downloads/AaKaiSong2.ttf EN=~/Downloads/QuattrocentoSans-Regular.ttf
pyftsubset $ZH --output-file=zh.ttf \
  --text=`cat main.lua | perl -CIO -pe 's/[\p{ASCII} \N{U+2500}-\N{U+257F}]//g'`
pyftsubset $EN --output-file=en.ttf \
  --unicodes=41-5a,2e
