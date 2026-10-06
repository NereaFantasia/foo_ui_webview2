export interface Events {
  /**
   * foobar2000 is exiting. Sent once, before any window or panel of the plugin closes, to every
   * page: the main window's, popups' and panels', whatever the interface and whether or not
   * background mode is on. Best effort only: the event is queued to each page and the host does
   * not wait. The pages close right after, so a handler may not run or finish, and a call it makes
   * to the host is unlikely to be answered.
   * @zh foobar2000 正在退出。在插件的任何窗口或面板关闭之前发一次，送到所有页面：主窗口、popup 与面板的页面，不论界面是哪个、是否开了后台模式。只是尽力而为：事件排进各页面的队列，宿主不等待。页面紧接着就会关闭，处理函数不一定来得及运行或跑完，它向宿主发出的调用多半得不到答复。
   * @delivery broadcast
   */
  beforeQuit: void;
}
