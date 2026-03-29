#include <app/viewer/Info.hpp>
#include <app/viewer/manager.hpp>

namespace VIEWER{

std::string Info::relative_path()const{
  manager&mgr=manager::get_instance();
  return std::filesystem::relative(this->path.path,mgr.base_dir).string();
}

} // namespace VIEWER