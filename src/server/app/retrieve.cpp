#include <string>
#include <string_view>
#include <vector>
#include <cctype>
#include <algorithm>
#include <memory>
#include <utility>
#include <stdexcept>

#include <app/retrieve.hpp>

namespace RETRIEVE{
using namespace std;

class parser{
	std::unique_ptr<QueryAST> parse_term(string_view&sv){
    if(type==token_t::kwlparen){
      get_token(sv); // (を消費して次のトークンを取得
      auto ptr=parse_or(sv);
      if(!ptr) return nullptr;
      get_token(sv); // )を消費して次のトークンを取得
      return ptr;
    }else if(type==token_t::kwnot){
      get_token(sv); // !を消費して次のトークンを取得
      auto ptr=parse_term(sv);
      if(!ptr) return nullptr;
      if(auto tptr=dynamic_cast<termAST*>(ptr.get()))
        return make_unique<termAST>(std::move(*tptr),true);
      return make_unique<termAST>(std::move(ptr),true);
    }else if(type==token_t::term){
      auto ptr=make_unique<termAST>(std::move(token), type==token_t::kwnot);
      get_token(sv); // 次のトークンを取得
      return ptr;
    } else return nullptr;
	}

	std::unique_ptr<QueryAST> parse_and(string_view&sv){
    vector<unique_ptr<QueryAST>> terms;
    terms.emplace_back(parse_term(sv));
    if(!terms.back()) return nullptr;
    // 空白はAND，空白はスキップされている
    // ORなら戻る，NOTや(はtermを呼び出す．
    while(type==token_t::kwand || type==token_t::kwnot || type==token_t::kwlparen || type==token_t::term){
      if(type==token_t::kwand) get_token(sv); // &を消費して次のトークンを取得
      terms.emplace_back(parse_term(sv));
      if(!terms.back()) return nullptr;
    }
    if(terms.size()==1) return move(terms.front());
    return make_unique<andAST>(std::move(terms));
	}

	std::unique_ptr<QueryAST> parse_or(string_view&sv){
    vector<unique_ptr<QueryAST>> terms;
    terms.emplace_back(parse_and(sv));
    if(!terms.back()) return nullptr;
    while(type==token_t::kwor){
      get_token(sv); // |を消費して次のトークンを取得
      terms.emplace_back(parse_and(sv));
      if(!terms.back()) return nullptr;
    }
    if(terms.size()==1) return move(terms.front());
    return make_unique<orAST>(std::move(terms));
	}
  
  void get_token(string_view&sv){
    skip_whitespace(sv);
    token.clear();
    if(sv.empty()){
      type=token_t::none;
      return;
    }
    // クォーテーションされていない以下の文字列をトークンとして扱う
    // ! ( ) & | && || AND OR NOT and or not
    // ただしアルファベットは大文字小文字を区別する
    // また，クォーテーションされている文字列はクォート記号を除外する
    // クォート記号は"のみとし，クォート文字列内ではエスケープする
    // 文字列の途中で"が出る場合は普通の文字として扱う
    switch(sv[0]){
      case '!':{ type=token_t::kwnot; sv.remove_prefix(1); }return;
      case '"':{
        size_t idx=1;
        bool escape=false;
        type=token_t::term;
        for(;idx<sv.size();++idx){
          if(escape){
            escape=false;
            if(sv[idx]!='"'||sv[idx]!='\\')
              token.push_back('\\');
            token.push_back(sv[idx]);
          }else{
            if(sv[idx]=='"') break;
            else if(sv[idx]=='\\') escape=true;
            else token.push_back(sv[idx]);
          }
        }
        if(escape) token.push_back('\\');
        sv.remove_prefix(std::min(idx,sv.size()));
      }return;
      case '&':{ type=token_t::kwand; sv.remove_prefix(1+(sv.size()>1&&sv[1]=='&')); }return;
      case '(':{ type=token_t::kwlparen; sv.remove_prefix(1); }return;
      case ')':{ type=token_t::kwrparen; sv.remove_prefix(1); }return;
      case '|':{ type=token_t::kwor; sv.remove_prefix(1+(sv.size()>1&&sv[1]=='|')); }return;
      default:{
        for(size_t idx=0;idx<sv.size();++idx){
          if(isspace(sv[idx])
            ||sv[idx]=='!'||sv[idx]=='('||sv[idx]==')'
            ||sv[idx]=='&'||sv[idx]=='|'
          ) break;
          token.push_back(sv[idx]);
        }
        sv.remove_prefix(token.size());
        if(token=="AND"||token=="and") type=token_t::kwand;
        else if(token=="OR"||token=="or") type=token_t::kwor;
        else if(token=="NOT"||token=="not") type=token_t::kwnot;
        else type=token_t::term;
      }return;
    }
  }

  void skip_whitespace(string_view&sv){
    while(sv.size()&&isspace(sv[0]))
      sv.remove_prefix(1);
  }

	public:
	std::unique_ptr<QueryAST> parse_query(string_view sv){
    get_token(sv);
    auto ptr=parse_or(sv);
    if(!ptr) return nullptr;
    return ptr;
	}

	private:
	enum class token_t{
		term, kwand, kwor, kwnot, kwlparen, kwrparen, none
	};
	token_t type;
	string token;
};

std::unique_ptr<QueryAST> parse_query(const string&s){
	parser p;
	return p.parse_query(s);
}
} // namespace RETRIEVE