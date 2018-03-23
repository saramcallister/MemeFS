# MemeFS: A File System for the Millenial
Sara McAllister, Jake Palanker, Gavin Yancey

## Overview:
MemeFS is a file system that puts your local meme folder to good use. We will hide all of your most secret files inside your memes (we do not promise any added security features in our file system or any other protection from adversaries). When you mount the file system on a local directory, we will show you all of these hidden files using an inode scheme.

## Basic parts of filesystem:
Using steganography, we can store an essentially unlimited amount of hidden data in a single image file.  However, if you put too much data in an image it starts to look suspicious.  As such, our filesystem will be split across many memes, all stored in a single "memes" folder.  For our initial filesystem, we will have the superblock stored in a meme with a known name, and will store configuration data and the name of the inode table meme.

  We're planning on having a simple inode-based filesystem, with a inode-table file storing inode metadata and which meme files each file's data is stored in.  For our initial implementation, we will store the data for each file in one meme, but an eventual stretch goal is to split large files across multiple memes and to combine multiple small files into fewer memes.

## Steganography:
When we write a file to MemeFS, we need an image in which to put the data. We plan on downloading the memes off the Internet from specific known meme pages. These memes will use as the images. Ideally in the long run, we would allow the user to specify some meme pages that they frequent to make the meme access pattern look less suspicious. We plan on filtering out any images that are not JPEGs, which will allow us to more easily ensure that the file is an image and to properly execute the steganography.

	Once we have the image, we will use steganography code implemented in C to hide the data. When we do not split files, this could make the hidden data unreasonably large, but we plan on eventually splitting the data between multiple memes. This means that to read a file, we will need to unhide the data which will be more time consuming than a normal read off the file. For writing to a file, we plan on completely deleting the affected meme and writing the entire block of data to a new meme which we will generate using the previously mentioned method. Based on in class discussions, this approach is in line with many UNIX programs which truncate and then rewrite entire files when they are modified.

## Proposed Additional Features (S T R E T C H Y   G O A L S)
In the basic system, our data is easily detectable. For small files, allocating only a single image to contain the entire file is acceptable, but with large files injecting the data into only one image would cause files to grow significantly in size, suspicious to any observer. Additionally our data is stored in plain text, which is bad. If we complete the original features with time to spare we hope to tackle these problems by spreading files over multiple meme blocks and adding an encryption layer.

  Our system, without any performance optimizations, will most likely be very slow. Our current ideas involve caching writes in memory for quite a while before flushing them to “disk” and storing small files in a different way, either having them share “blocks” with other small files, or being placed into the metadata itself. 
  
  In addition to new features, we would also like to add more customizability to the existing features, allowing a user to define how they would like to trade off between discoverability and performance. Additionally, as many people are very picky about their memes, being able to customize the source might be a good idea

## Sources / Research:
https://github.com/StefanoDeVuono/steghide 
https://github.com/samuelcouch/c-steganography 
https://www.reddit.com/wiki/rss 


