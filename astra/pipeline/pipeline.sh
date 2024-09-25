# export PATH=./pnm_conv:$PATH

img_path=../img
img_proc=../img-processed

solve_field=${solve_field:-../../aux/astrometry.net-install/bin/solve-field}

if [ ! -d "$img_proc" ]; then
  mkdir $img_proc
fi

if [ ! -z "$1" ]; then
  n=$1
  ../align/align \
    $img_proc/$n.png \
    $img_proc/$n.axy \
    $img_proc/$n.rdls \
    $img_proc/$n-indx.xyls \
    $img_proc/$n.corr \
    $img_proc/$n.wcs \
    $img_proc/$n.refi \
    $img_proc/$n.coeff
  exit
fi

if [ ! -f ./selcrop ]; then
  echo "Please build ./selcrop first"
  exit 1
elif [ ! -f ../align/align ]; then
  echo "Please build ../align/align first"
  exit 1
elif ! command $solve_field &>/dev/null; then
  echo "Command \"$solve_field\" is not valid. Please properly set \$solve_field to point to the astrometry.net installation."
  exit 1
fi

images=`find $img_path \( -name "*.jpg" -o -name "*.jpeg" -o -name "*.png" \)`

echo "(1/3) Crop"
for i in $images; do
  bn=`basename $i`
  n=${bn%.*}
  if [ ! -f "$img_proc/$n.png" ]; then
    echo $bn
    ./selcrop $i $img_proc/$n.png || break
  fi
done

echo "(2/3) Solve"
for i in $images; do
  bn=`basename $i`
  n=${bn%.*}
  if [ ! -f "$img_proc/$n.tried" ]; then
    echo $bn
    $solve_field --scale-low 10 --tweak-order 2 --crpix-center --parity neg $img_proc/$n.png --overwrite
    touch $img_proc/$n.tried
  fi
done

echo "(3/3) Refine alignment"
for i in $images; do
  bn=`basename $i`
  n=${bn%.*}
  if [ -f "$img_proc/$n.solved" ] && [ ! -f "$img_proc/$n.coeff" ]; then
    echo $bn
    ../align/align \
      $img_proc/$n.png \
      $img_proc/$n.axy \
      $img_proc/$n.rdls \
      $img_proc/$n-indx.xyls \
      $img_proc/$n.corr \
      $img_proc/$n.wcs \
      $img_proc/$n.refi \
      $img_proc/$n.coeff
  fi
done
