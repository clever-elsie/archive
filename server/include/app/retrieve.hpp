#pragma once
#include <string>
#include <memory>
#include <variant>

namespace RETRIEVE{
using namespace std;

struct Retrieval{
	virtual bool match(const string&s) const = 0;
};

struct QueryAST{
	constexpr static size_t indent_size = 2;
	virtual ~QueryAST()=default;
	virtual bool evaluate(const Retrieval&tar)const = 0;
	virtual vector<string> to_string(size_t indent=0)const = 0;
};

struct termAST : public QueryAST {
	termAST(string&&token_, bool not_flag_):not_flag(not_flag_), child_or_token(move(token_)){}
	termAST(const string&token_, bool not_flag_):not_flag(not_flag_), child_or_token(token_){}
	termAST(std::unique_ptr<QueryAST>&&child_, bool not_flag_):not_flag(not_flag_), child_or_token(move(child_)){}
	termAST(termAST&&other, bool not_flag_):not_flag(not_flag_), child_or_token(move(other.child_or_token)){}
	virtual bool evaluate(const Retrieval&tar)const override{
		bool ret = false;
		if(std::holds_alternative<string>(child_or_token))
			ret = tar.match(std::get<string>(child_or_token));
		else ret = std::get<std::unique_ptr<QueryAST>>(child_or_token)->evaluate(tar);
		return not_flag ^ ret; // NOTがある時反転
	}
	virtual vector<string> to_string(size_t indent=0)const override{
		vector<string>ret;
		ret.emplace_back(string(indent,' ')+string(not_flag ? "!" : "")+"TERM{");
		if(std::holds_alternative<unique_ptr<QueryAST>>(child_or_token)){
			vector<string> child_ret = std::get<std::unique_ptr<QueryAST>>(child_or_token)->to_string(indent+QueryAST::indent_size);
			for(auto&&line:child_ret)
				ret.emplace_back(move(line));
			ret.emplace_back(string(indent,' ')+"}");
		}else ret.back().insert(ret.back().size(),std::get<string>(child_or_token)+"}");
		return ret;
	}
	private:
	bool not_flag;
	std::variant<string,std::unique_ptr<QueryAST>> child_or_token;
};

struct andAST : public QueryAST {
	andAST(vector<std::unique_ptr<QueryAST>>&&terms_):terms(move(terms_)){}
	virtual bool evaluate(const Retrieval&tar)const override{
		for(const auto&term:terms)
			if(!term->evaluate(tar))
				return false;
		return true;
	}
	virtual vector<string> to_string(size_t indent=0)const override{
		if(terms.size()==1) return terms[0]->to_string(indent);
		vector<string>ret;
		ret.emplace_back(string(indent,' ')+"AND{");
		for(const auto&term:terms){
			vector<string> child_ret = term->to_string(indent+QueryAST::indent_size);
			for(auto&&line:child_ret)
				ret.emplace_back(move(line));
		}
		ret.emplace_back(string(indent,' ')+"}");
		return ret;
	}
	private:
	vector<std::unique_ptr<QueryAST>> terms;
};

struct orAST : public QueryAST {
	orAST(vector<std::unique_ptr<QueryAST>>&&terms_):terms(move(terms_)){}
	virtual bool evaluate(const Retrieval&tar)const override{
		if(terms.empty()) return true;
		for(const auto&term:terms)
			if(term->evaluate(tar))
				return true;
		return false;
	}
	virtual vector<string> to_string(size_t indent=0)const override{
		if(terms.size()==1) return terms[0]->to_string(indent);
		vector<string>ret;
		ret.emplace_back(string(indent,' ')+"OR{");
		for(const auto&term:terms){
			vector<string> child_ret = term->to_string(indent+QueryAST::indent_size);
			for(auto&&line:child_ret)
				ret.emplace_back(move(line));
		}
		ret.emplace_back(string(indent,' ')+"}");
		return ret;
	}
	private:
	vector<std::unique_ptr<QueryAST>> terms;
};

/**
 * @return nullptrならパースエラー
 */
std::unique_ptr<QueryAST> parse_query(const string&s);

} // namespace RETRIEVE
