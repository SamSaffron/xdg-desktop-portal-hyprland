# select either qt6 or qt5
qtdep = dependency('qt6', 'qt5', modules: ['Widgets'])
qtver = qtdep.version()
qt = import('qt' + qtver[0])

hyprutils = dependency('hyprutils', version: '>=0.2.6')
wayland_client = dependency('wayland-client')

ui_files = qt.compile_ui(sources: 'mainpicker.ui')
moc = qt.compile_moc(headers: ['mainpicker.h', 'waylandcapture.h'], dependencies: [client_protos, hyprutils])

sources = files([
  'main.cpp',
  'mainpicker.cpp',
  'mainpicker.h',
  'elidedbutton.h',
  'elidedbutton.cpp',
  'waylandcapture.cpp',
  'waylandcapture.h',
])

executable('hyprland-share-picker',
  sources,
  ui_files,
  moc,
  include_directories: inc,
  dependencies: [qtdep, client_protos, hyprutils, wayland_client],
  install: true
)
