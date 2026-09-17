find_package(GUIForms 0.1 CONFIG REQUIRED COMPONENTS Application)
include(CheckCXXSourceCompiles)
set(CMAKE_REQUIRED_LIBRARIES GUIForms::Application)
check_cxx_source_compiles("#include <gui_forms/canvas.hpp>
#include <type_traits>
static_assert(!std::is_final_v<gui_forms::RasterCanvas>);
int main() { return 0; }" RAINSTAR_EXTENSIBLE_CANVAS)
check_cxx_source_compiles("#include <gui_forms/basic_controls.hpp>
#include <gui_forms/application.hpp>
int main() {
  gui_forms::DropDownButton button(gui_forms::StableId(\"ribbon\"));
  button.set_drop_down_edge(gui_forms::DropDownButtonEdge::bottom);
  gui_forms::ApplicationWindowHandle handle;
  static_cast<void>(handle.toggle_full_screen());
  return 0;
}" RAINSTAR_RIBBON_CONTROLS)
unset(CMAKE_REQUIRED_LIBRARIES)
if(NOT RAINSTAR_EXTENSIBLE_CANVAS)
  message(FATAL_ERROR "Rainstar Paint requires GUI.Forms with the RasterCanvas extension (7444815 or later). See docs/GUI_FORMS_PORT.md.")
endif()
if(NOT RAINSTAR_RIBBON_CONTROLS)
  message(FATAL_ERROR "Rainstar Paint requires the GUI.Forms ribbon composition extension. See docs/GUI_FORMS_PORT.md.")
endif()
add_library(paint_forms STATIC src/forms/editor.cpp src/forms/display.cpp src/forms/ribbon.cpp src/forms/dialog.cpp)
target_link_libraries(paint_forms PUBLIC paint_core GUIForms::Application)
target_include_directories(paint_forms PUBLIC src)
add_executable(rainstar-paint-forms MACOSX_BUNDLE src/forms/main.cpp)
target_link_libraries(rainstar-paint-forms PRIVATE paint_forms)
set_target_properties(rainstar-paint-forms PROPERTIES
  MACOSX_BUNDLE_BUNDLE_NAME "Rainstar Paint GUI.Forms"
  MACOSX_BUNDLE_GUI_IDENTIFIER "org.rainstar.paint.forms"
  MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
  MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}")
if(APPLE)
  enable_language(OBJCXX)
  target_sources(paint_forms PRIVATE src/platform_mac.mm)
  target_link_libraries(paint_forms PRIVATE "-framework Cocoa" "-framework UniformTypeIdentifiers")
  target_compile_definitions(paint_forms PRIVATE RAINSTAR_FORMS_NATIVE_PRINT=1)
  file(GLOB toolkit_fonts CONFIGURE_DEPENDS "${GUIForms_FONT_DIR}/*")
  set_source_files_properties(${toolkit_fonts} PROPERTIES MACOSX_PACKAGE_LOCATION "Resources/fonts")
  target_sources(rainstar-paint-forms PRIVATE ${toolkit_fonts} assets/app-icon.icns)
  set_source_files_properties(assets/app-icon.icns PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
  set_target_properties(rainstar-paint-forms PROPERTIES MACOSX_BUNDLE_ICON_FILE app-icon.icns)
endif()
if(WIN32)
  target_sources(paint_forms PRIVATE src/print_windows.cpp)
  target_link_libraries(paint_forms PRIVATE comdlg32 gdi32)
  target_compile_definitions(paint_forms PRIVATE RAINSTAR_FORMS_NATIVE_PRINT=1)
endif()
if(NOT MSVC)
  target_compile_options(paint_forms PRIVATE -Wall -Wextra -Wpedantic)
endif()
add_executable(paint-forms-tests tests/forms_tests.cpp)
target_link_libraries(paint-forms-tests PRIVATE paint_forms)
add_test(NAME paint-forms COMMAND paint-forms-tests)
install(TARGETS rainstar-paint-forms BUNDLE DESTINATION . RUNTIME DESTINATION .)
if(APPLE OR WIN32)
  add_executable(paint-forms-native-tests MACOSX_BUNDLE tests/forms_native.cpp)
  target_link_libraries(paint-forms-native-tests PRIVATE paint_forms)
  if(APPLE)
    target_sources(paint-forms-native-tests PRIVATE ${toolkit_fonts})
    set_target_properties(paint-forms-native-tests PROPERTIES
      MACOSX_BUNDLE_GUI_IDENTIFIER "org.rainstar.paint.forms.validation")
  endif()
  add_test(NAME paint-forms-native COMMAND paint-forms-native-tests)
endif()
# Explicit offline authoring target; not part of application builds or installation.
if(EXISTS "${PROJECT_SOURCE_DIR}/astra/ribbon-export.cpp")
  add_executable(paint-export-ribbon-icons EXCLUDE_FROM_ALL astra/ribbon-export.cpp)
  target_include_directories(paint-export-ribbon-icons PRIVATE src scripts)
  target_link_libraries(paint-export-ribbon-icons PRIVATE paint_core)
endif()
