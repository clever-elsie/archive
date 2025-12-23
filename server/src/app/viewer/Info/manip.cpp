#include <app/viewer/Info.hpp>

namespace VIEWER{

namespace {
  using array_5 = std::array<std::string_view, 5>;
  using pair_array = std::pair<array_5, size_t>;
  constexpr static std::array<pair_array, (size_t)Info::MediaType::size_> exts{{
    {{".webp",".jpg",".jpeg",".png",".gif"}, 5},
    {{".mp4","","","",""}, 1},
    {{".mp3",".flac",".aac",".wav",""}, 4},
    {{".txt",".md","","",""}, 2},
    {{".pdf","","","",""}, 1}
  }};
}

Info::MediaType Info::classify(const std::filesystem::path& filename){
  const std::string ext = filename.extension().string();
  if(ext.empty()) return MediaType::size_;
  for(size_t i=0;i<exts.size();++i)
    if(std::ranges::contains(exts[i].first,ext))
      return Info::MediaType(i);
  return MediaType::size_;
}

void Info::classify_and_push(const std::filesystem::path& filename, MediaArray& to_ins){
  const MediaType mt=classify(filename);
  if(MediaType::size_==mt) return;
  to_ins[Info::mt_index(mt)].emplace_back(filename.filename().string());
}

void Info::classify_and_push(const std::filesystem::path& filename, MediaArray& media, MediaArray& to_ins){
  const MediaType mt=classify(filename);
  if(MediaType::size_==mt) return;
  const size_t mti=Info::mt_index(mt);
  if(std::lower_bound(media[mti].begin(),media[mti].end(), filename.filename().string())!=media[mti].end())
    return; // 既に存在するときは何もしない
  to_ins[mti].emplace_back(filename.filename().string());
}

} // namespace VIEWER