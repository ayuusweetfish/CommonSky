line=$(curl https://nova.astrometry.net/user_images/$1 | grep 'var images =')
img=$(echo "${line}; console.log(images.original);" | deno run /dev/stdin)
curl https://nova.astrometry.net$img -o ../img/astrometry.net-$1.jpg
echo ../img/astrometry.net-$1.jpg
