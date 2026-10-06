# 给组件加样式

主题不加样式时，`fb-*` 元素几乎什么都不画。每个组件的参考条目列出了它的 part、slot 和宿主属性；本页演示这三种挂钩怎么用。组件为什么不带样式，见[组件的工作方式](/zh/concepts/components)。

## 用 `::part()` 给内部元素加样式

在样式表里选中元素的某个 part：

```css
/* 圆形的播放按钮 */
fb-play-button::part(button) {
    width: 48px;
    height: 48px;
    border: none;
    border-radius: 50%;
    background: #1db954;
    color: white;
    cursor: pointer;
}
fb-play-button::part(button):hover {
    background: #1ed760;
}

/* 进度条有轨道、填充段和滑块三个 part */
fb-seek-bar::part(track) {
    background: linear-gradient(#333, #333) center / 100% 4px no-repeat;
}
fb-seek-bar::part(fill) { height: 4px; background: #1db954; }
fb-seek-bar::part(thumb) { width: 12px; height: 12px; border-radius: 50%; background: #fff; }
```

`::part()` 只能选中具名的那个元素，选不到它的后代，所以要直接给 part 本身显示的内容加样式。

## 用宿主属性响应状态

组件把状态反映为元素上的属性，和 `::part()` 组合使用：

```css
/* 播放中时突出播放按钮 */
fb-play-button[playing]::part(button) {
    background: #ffffff;
    color: #1db954;
}

/* 静音时把音量控件调暗 */
fb-volume-control[muted] {
    opacity: 0.5;
}
```

`::part()` 可以与宿主属性组合（如上），也可以接 `:hover`、`:focus-visible` 这类用户操作伪类。`::part()` 后面接属性选择器不是合法的 CSS，所以组件标在内部元素上的状态，例如 `<fb-rating>` 星星上的 `data-filled`，页面选不到。

## 用 slot 替换内容

把自己的标记放进具名 slot，替换默认内容：

```html
<fb-play-button>
    <svg slot="play-icon" viewBox="0 0 24 24" width="20" height="20"><path d="M8 5v14l11-7z" /></svg>
    <svg slot="pause-icon" viewBox="0 0 24 24" width="20" height="20"><path d="M6 5h4v14H6zm8 0h4v14h-4z" /></svg>
</fb-play-button>
```

显示哪个 slot 仍由组件决定；这里是播放中显示暂停图标。

## 把样式放在一处

组件的样式就是普通的页面 CSS，和其他样式一起放在主题的样式表里；在 `:root` 上设的 CSS 自定义属性（比如强调色）也和其他规则一样作用到它们。[构建第一个主题](/zh/tutorials/first-theme)就是这样给整个播放器加样式的。
