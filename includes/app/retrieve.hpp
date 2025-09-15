#pragma once
#include <ranges>
#include <string>
#include <cstddef>
#include <cstdint>
#include <concepts>
#include <type_traits>

namespace RETRIEVE{
using namespace std;

/**
 * @brief 現在トークンを返し次のトークンの先頭を返す
 * @param idx 現在最小すべき先頭のインデックス
 * @param s idxから始まるトークン
 * @return 次のトークンの先頭インデックスとトークン
 */
inline pair<size_t,string> get_token(size_t idx,const string&s){
	size_t cnt=0;
	string ret;
	bool token=true;
	for(const auto&x:s|views::drop(idx)){
		if(token){
			if(x==' ') token=false;
			else if(x=='('||x==')'||x=='!')break;
			else ret.push_back(x);
		}else if(x!=' ')break;
		++cnt;
	}
	return{cnt,ret};
}

/**
 * @brief クエリをパースする．or_expr=falseのとき空白をANDとみなし，そうでないときORとみなす．!はNOTである．
 * @param Info 検索対象のデータ型．tagとpathがcontainsを持つ必要がある．
 * @param or_expr AND/ORのフラグ
 * @param idx クエリの読み込み位置
 * @param tar 検索対象のデータ．
 * @param s クエリ
 * @return クエリにマッチしたときtrue
 */
template<class Info,bool or_exp=false>
requires requires(Info&tar,const string&s){
  tar.tag.contains(s);
  tar.path.contains(s);
}
inline bool parse_query(size_t&idx,const Info&tar,const string&s){
	bool nx_not=false;
	while(idx<s.size()){
		if(s[idx]=='!'){
			nx_not=!nx_not;
			++idx;
			continue;
		}
		bool r;
		switch(s[idx]){
			case '(': r=parse_query<Info,!or_exp>(++idx,tar,s); break;
			case ')': ++idx;return !or_exp;
			default:{
				auto[cnt,token]=get_token(idx,s);
				idx+=cnt;
				r=tar.tag.contains(token)||tar.path.contains(token);
			}break;
		}
		if(nx_not) r=!r;
		if constexpr(or_exp){
			if(r) return true;
		}else{ if(!r) return false; }
	}
	return !or_exp;
}
} // namespace RETRIEVE