dirload-cpp ([v0.4.0](https://github.com/kusumi/dirload-cpp/releases/tag/v0.4.0))
========

## About

+ Set read / write workloads on a file system.

+ C++ version of [https://github.com/kusumi/dirload-rs](https://github.com/kusumi/dirload-rs).

## Supported platforms

Unix-likes in general

## Requirements

C++20

## Build

    $ make

## Usage

    $ ./build/src/dirload-cpp -h
    Usage: ./build/src/dirload-cpp [options] <paths>
    Options:
      --num_set - Number of sets to run (default 1)
      --num_reader - Number of reader threads
      --num_writer - Number of writer threads
      --num_repeat - Exit threads after specified iterations if > 0 (default -1)
      --time_minute - Exit threads after sum of this and --time_second option if > 0
      --time_second - Exit threads after sum of this and --time_minute option if > 0
      --monitor_interval_minute - Monitor threads every sum of this and --monitor_interval_second option if > 0
      --monitor_interval_second - Monitor threads every sum of this and --monitor_interval_minute option if > 0
      --stat_only - Do not read file data
      --ignore_dot - Ignore entries start with .
      --follow_symlink - Follow symbolic links for read unless directory
      --read_buffer_size - Read buffer size (default 65536)
      --read_size - Read residual size per file read, use < read_buffer_size random size if 0 (default -1)
      --write_buffer_size - Write buffer size (default 65536)
      --write_size - Write residual size per file write, use < write_buffer_size random size if 0 (default -1)
      --random_write_data - Use pseudo random write data
      --num_write_paths - Exit writer threads after creating specified files or directories if > 0 (default 1024)
      --truncate_write_paths - ftruncate(2) write paths for regular files instead of write(2)
      --fsync_write_paths - fsync(2) write paths
      --dirsync_write_paths - fsync(2) parent directories of write paths
      --keep_write_paths - Do not unlink write paths after writer threads exit
      --clean_write_paths - Unlink existing write paths and exit
      --write_paths_base - Base name for write paths (default x)
      --write_paths_type - File types for write paths [d|r|s|l] (default dr)
      --path_iter - <paths> iteration type [walk|ordered|reverse|random] (default ordered)
      --flist_file - Path to flist file
      --flist_file_create - Create flist file and exit
      --force - Enable force mode
      --verbose - Enable verbose print
      --debug - Enable debug mode
      -v, --version - Print version and exit
      -h, --help - Print usage and exit
