## TzData prebuilts in HOS format

Based off tzdb 2021a

Generate as normal then
```
rm *.tab
rm leapseconds

find -type f | sed 's/\.\///g' | grep -v '\.' | sort > ../binaryList.txt
```
