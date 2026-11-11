export interface Events {
  /**
   * foobar2000 is exiting. Sent once, before any window or panel of the plugin closes, to every
   * page: the main window's, popups' and panels', whatever the interface and whether or not
   * background mode is on. Delivery is best-effort: the event is queued to each page without
   * awaiting handlers. The pages close right after, so a handler may not run or finish, and a
   * host call it makes may go unanswered.
   * @zh foobar2000 正在退出。该事件在组件的所有窗口及面板关闭前向所有页面派发一次（包含主窗口、弹窗和各类面板，不论当前使用哪种界面、是否开启后台模式）。该事件为尽力交付（best-effort）模式：宿主将事件加入页面队列后即继续退出流程，不会等待页面处理完成。页面随即关闭，事件监听器可能来不及运行或执行完毕，其中发出的 API 调用也可能收不到应答。
   * @delivery broadcast
   */
  beforeQuit: void;
}
