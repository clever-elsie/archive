#pragma once
#include <cstddef>
#include <cstdint>
#include <chrono>
#include <cassert>
#include <stdexcept>

#include <mutex>

#include <tuple>
#include <vector>
#include <utility>

#include <ranges>
#include <array>
#include <numeric>
#include <algorithm>
#include <functional>

#include <concepts>
#include <iterator>
#include <type_traits>

/*
 * expireはパスワード試行回数制限などの制限を柔軟に管理するためのクラスである．
 * 回数に応じたペナルティを儲けることができる
 */

namespace expire{
/*
 * info_tは制限の情報を持つ構造体である．
 * 拒否期限は開示しない
 */
struct default_penalty{
  constexpr static
  std::chrono::system_clock::duration operator()(uint64_t tried)noexcept{
    using namespace std;
    using namespace chrono;
    static constexpr array<pair<uint64_t, system_clock::duration>, 10> penalties = {
      pair<uint64_t, system_clock::duration>
      {0, seconds{0}}, {5, minutes{1}}, {10, hours{1}}, {20, hours{12}},
      {30, days{1}}, {40, weeks{1}}, {50, months{1}}, {60, years{1}},
      {70, years{10}}, {80, years{100}}
    };
    auto it=ranges::upper_bound(penalties, tried, {}, &pair<uint64_t, system_clock::duration>::first);
    return (--it)->second;
  }
};

template<class penalty_t=default_penalty>
requires requires(const penalty_t&pen){
  { pen(uint64_t{}) } -> std::same_as<std::chrono::system_clock::duration>;
}
class info_t{
  public:
  using tried_t=uint64_t;
  using time_point=std::chrono::system_clock::time_point;
  info_t(const info_t&)=default;
  info_t(info_t&&)=default;
  info_t&operator=(const info_t&)=default;
  info_t&operator=(info_t&&)=default;
  info_t():tried{},locked_until{},penalty(penalty_t{}){}
  info_t(const penalty_t&pen):tried{},locked_until{},penalty(pen){}
  info_t(penalty_t&&pen):tried{},locked_until{},penalty(std::move(pen)){}

  template<class Pred, class...Args>
  bool challenge(Pred&&pred, Args&&...args){
    if(!under_penalty() && pred(std::forward<Args>(args)...)){
      reset();
      return true;
    }
    sanction();
    return false;
  }
  bool under_penalty()const noexcept{
    return std::chrono::system_clock::now()<=locked_until;
  }
  private:
  void reset()noexcept{
    tried=0;
    locked_until=time_point{};
  }
  void sanction(){
    locked_until=std::chrono::system_clock::now()+penalty(++tried);
  }
  private:
  tried_t tried;
  time_point locked_until;
  [[no_unique_address]] penalty_t penalty;
};

template<class T, class Info,
  template<class...> class Container,
  class... ContainerArgs
>
requires requires(const Container<T,Info,ContainerArgs...>&cont){
  std::same_as<typename Container<T,Info,ContainerArgs...>::key_type,T>;
  std::same_as<typename Container<T,Info,ContainerArgs...>::value_type,std::pair<const T,Info>>;
  { cont.find(std::declval<T>()) } -> std::same_as<typename Container<T,Info,ContainerArgs...>::iterator>;
  { cont.end() } -> std::same_as<typename Container<T, Info, ContainerArgs...>::const_iterator>;
}
class Traits{
  public:
  using container_type=Container<T,Info,ContainerArgs...>;
  using info_type=Info;
  using key_type=T;
  using mapped_type=info_type;
  using value_type=std::pair<const T,info_type>;
  using iterator=typename Container<T,info_type,ContainerArgs...>::iterator;
  using const_iterator=typename Container<T,info_type,ContainerArgs...>::const_iterator;
  using reference=typename Container<T,info_type,ContainerArgs...>::reference;
};

template<class traits>
concept is_valid_traits=
  requires(const traits&tr){
    typename traits::info_type;
    typename traits::container_type;
    typename traits::key_type;
    typename traits::mapped_type;
    typename traits::value_type;
    typename traits::iterator;
    typename traits::const_iterator;
    typename traits::reference;
  } && requires(traits::container_type&cont){
    { cont.find(std::declval<typename traits::key_type>()) } -> std::same_as<typename traits::iterator>;
    { cont.end() } -> std::same_as<typename traits::const_iterator>;
    cont.emplace(std::declval<typename traits::key_type>(),std::declval<typename traits::mapped_type>());
  };

template<class T, class Info, class... Args>
using map_traits=Traits<T,Info,std::map,Args...>;

template<class T, class Info, class... Args>
using unordered_map_traits=Traits<T,Info,std::unordered_map,Args...>;


/*
 * cacheは制限の情報を管理するクラス．
 * Tは制限する対象の型．IDとかならuint64_tとか，ユーザー名ならstd::stringとか．
 * 制限はinfo_tを個別に設定するが，省略すればデフォルト値を用いる
 * 制限のデフォルト値はコンストラクタで指定する．指定しない場合はinfo_t{}である
 * 試行回数の検査は唯一であることが望ましいので基本的にコピー禁止
 * 必要に応じてmoveを使うべき．
 */
// デフォルトではstd::unordered_mapを使用
// std::mapとかコンパイル時にコンテナを変更可能に
template<class traits=map_traits<std::string,info_t<default_penalty>>>
requires is_valid_traits<traits>
class cache{
  typename traits::info_type default_info;
  typename traits::container_type cache_map;
  std::mutex mtx;
  public:
  cache()=default;
  cache(const cache&)=delete; // コピー禁止
  cache& operator=(const cache&)=delete; // 代入禁止
  cache(cache&&)=default; // moveコンストラクタ
  cache& operator=(cache&&)=default; // move代入演算子
  cache(const traits::info_type&info):default_info(info),cache_map(){}
  cache(traits::info_type&&info):default_info(std::move(info)),cache_map(){}
  
  template<class S>
  requires requires(const S&key){
    { cache_map.find(key) } -> std::same_as<typename traits::iterator>;
  }
  bool exists(const S&key)const{
    std::lock_guard<std::mutex> lock(mtx);
    return cache_map.find(key)!=cache_map.end();
  }
  
  template<class S>
  requires requires(const S&key){
    { cache_map.find(key) } -> std::same_as<typename traits::iterator>;
  }
  bool add(const S&key, const traits::info_type&info=default_info){
    std::lock_guard<std::mutex> lock(mtx);
    if(cache_map.find(key)!=cache_map.end()) return false;
    auto [_, success]=cache_map.emplace(key,info);
    return success;
  }

  template<class S, class Pred, class... Args>
  requires requires(const S&key){
    { cache_map.find(key) } -> std::same_as<typename traits::iterator>;
  }
  bool challenge(const S&key, Pred&&pred, Args&&...args){
    std::lock_guard<std::mutex> lock(mtx);
    auto it=cache_map.find(key);
    if(it==cache_map.end()){
      cache_map.emplace(key,default_info);
      it=cache_map.find(key);
    }
    return it->second.challenge(std::forward<Pred>(pred), std::forward<Args>(args)...);
  }
};

} // namespace expire