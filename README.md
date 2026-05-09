# OS Final Project - Web Clickstream Pipeline

## How To Compile:
First open this project folder in the terminal by right-clicking the mouse and then run:

make

## How To Run:
./run.sh -i data/ -o output/ -n 4

## Or manually:
./dispatcher data/ output/ 4 10

## Clean:
make clean

## How to view Log Files:
For full output of every file/project/requirement, Run these commands:

cat logs/ingester.log
cat logs/processor.log
cat logs/reporter.log
