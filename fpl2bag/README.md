# Fixposition ROS Bag Extractor

This is a command line utility to extract data from a Fixposition ".fpl" logfile to a ROS bag.

### Dependencies

- ROS (roscpp and topic_tools)
- glibc

### Build

```sh
catkin build fpl2bag
```

### Usage

```sh
rosrun fpl2bag fpl2bag -i some_log.fpl -o some_log.bag
```

## License

MIT, see [LICENSE](LICENSE)

