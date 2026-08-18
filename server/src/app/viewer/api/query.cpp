#include <app/viewer/api/query.hpp>

#include <algorithm>
#include <cctype>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <app/viewer/api/entry_json.hpp>
#include <app/viewer/ordering.hpp>

namespace VIEWER::api {

struct SearchExpression final {
  enum class Kind : std::uint8_t {
    term,
    and_,
    or_,
    not_
  };

  Kind kind = Kind::term;
  SearchField field = SearchField::any;
  std::string value;
  std::vector<std::shared_ptr<const SearchExpression>> children;
};

namespace {

std::string lower_copy(std::string_view value) {
  std::string result;
  result.reserve(value.size());
  for (const auto character : value)
    result.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(character))));
  return result;
}

bool equals_ascii_case_insensitive(std::string_view left, std::string_view right) {
  if (left.size() != right.size()) return false;
  for (std::size_t i = 0; i < left.size(); ++i) {
    const auto lhs = static_cast<unsigned char>(left[i]);
    const auto rhs = static_cast<unsigned char>(right[i]);
    if (std::tolower(lhs) != std::tolower(rhs)) return false;
  }
  return true;
}

enum class SearchTokenKind : std::uint8_t {
  end,
  term,
  and_,
  or_,
  not_,
  left_paren,
  right_paren,
  invalid
};

struct SearchToken final {
  SearchTokenKind kind = SearchTokenKind::invalid;
  SearchField field = SearchField::any;
  std::string value;
};

class SearchParser final {
public:
  explicit SearchParser(std::string_view input) : input_(input) {}

  std::optional<SearchQuery> parse() {
    next();
    const auto root = parse_or();
    if (!root || current_.kind != SearchTokenKind::end) return std::nullopt;
    return SearchQuery{*root};
  }

private:
  using Expression = std::shared_ptr<const SearchExpression>;

  static bool starts_unary(SearchTokenKind kind) {
    return kind == SearchTokenKind::term || kind == SearchTokenKind::not_ ||
           kind == SearchTokenKind::left_paren;
  }

  static Expression term_expression(SearchField field, std::string value) {
    auto expression = std::make_shared<SearchExpression>();
    expression->field = field;
    expression->value = std::move(value);
    return expression;
  }

  static Expression compound_expression(SearchExpression::Kind kind,
                                         std::vector<Expression> children) {
    auto expression = std::make_shared<SearchExpression>();
    expression->kind = kind;
    expression->children = std::move(children);
    return expression;
  }

  std::optional<Expression> parse_or() {
    auto first = parse_and();
    if (!first) return std::nullopt;

    std::vector<Expression> terms;
    terms.push_back(*first);
    while (current_.kind == SearchTokenKind::or_) {
      next();
      auto next_term = parse_and();
      if (!next_term) return std::nullopt;
      terms.push_back(*next_term);
    }
    if (terms.size() == 1) return terms.front();
    return compound_expression(SearchExpression::Kind::or_, std::move(terms));
  }

  std::optional<Expression> parse_and() {
    auto first = parse_unary();
    if (!first) return std::nullopt;

    std::vector<Expression> terms;
    terms.push_back(*first);
    while (true) {
      if (current_.kind == SearchTokenKind::and_) {
        next();
        if (!starts_unary(current_.kind)) return std::nullopt;
      } else if (!starts_unary(current_.kind)) {
        break;
      }

      auto next_term = parse_unary();
      if (!next_term) return std::nullopt;
      terms.push_back(*next_term);
    }
    if (terms.size() == 1) return terms.front();
    return compound_expression(SearchExpression::Kind::and_, std::move(terms));
  }

  std::optional<Expression> parse_unary() {
    if (current_.kind == SearchTokenKind::not_) {
      next();
      auto child = parse_unary();
      if (!child) return std::nullopt;
      std::vector<Expression> children{*child};
      return compound_expression(SearchExpression::Kind::not_, std::move(children));
    }
    return parse_primary();
  }

  std::optional<Expression> parse_primary() {
    if (current_.kind == SearchTokenKind::term) {
      auto expression = term_expression(current_.field, std::move(current_.value));
      next();
      return expression;
    }
    if (current_.kind != SearchTokenKind::left_paren) return std::nullopt;

    next();
    auto expression = parse_or();
    if (!expression || current_.kind != SearchTokenKind::right_paren) return std::nullopt;
    next();
    return expression;
  }

  void skip_whitespace() {
    while (position_ < input_.size() &&
           std::isspace(static_cast<unsigned char>(input_[position_]))) {
      ++position_;
    }
  }

  std::optional<std::string> read_quoted() {
    if (position_ >= input_.size() || input_[position_] != '"') return std::nullopt;
    ++position_;
    std::string value;
    bool escaped = false;
    while (position_ < input_.size()) {
      const char character = input_[position_++];
      if (escaped) {
        if (character != '"' && character != '\\') value.push_back('\\');
        value.push_back(character);
        escaped = false;
      } else if (character == '\\') {
        escaped = true;
      } else if (character == '"') {
        return value;
      } else {
        value.push_back(character);
      }
    }
    return std::nullopt;
  }

  SearchToken word_token(std::string value) {
    if (value.empty()) return {SearchTokenKind::invalid, SearchField::any, {}};

    SearchField field = SearchField::any;
    std::string_view suffix(value);
    if (suffix.size() > 4 && equals_ascii_case_insensitive(suffix.substr(0, 4), "tag:")) {
      field = SearchField::tag;
      suffix.remove_prefix(4);
    } else if (suffix.size() > 5 &&
               equals_ascii_case_insensitive(suffix.substr(0, 5), "path:")) {
      field = SearchField::path;
      suffix.remove_prefix(5);
    } else if (equals_ascii_case_insensitive(suffix, "tag:") ||
               equals_ascii_case_insensitive(suffix, "path:")) {
      return {SearchTokenKind::invalid, SearchField::any, {}};
    } else if (equals_ascii_case_insensitive(value, "AND")) {
      return {SearchTokenKind::and_, SearchField::any, {}};
    } else if (equals_ascii_case_insensitive(value, "OR")) {
      return {SearchTokenKind::or_, SearchField::any, {}};
    } else if (equals_ascii_case_insensitive(value, "NOT")) {
      return {SearchTokenKind::not_, SearchField::any, {}};
    }
    if (suffix.empty()) return {SearchTokenKind::invalid, SearchField::any, {}};
    return {SearchTokenKind::term, field, std::string(suffix)};
  }

  SearchToken read_token() {
    skip_whitespace();
    if (position_ >= input_.size()) return {SearchTokenKind::end, SearchField::any, {}};

    switch (input_[position_]) {
      case '!':
        ++position_;
        return {SearchTokenKind::not_, SearchField::any, {}};
      case '(':
        ++position_;
        return {SearchTokenKind::left_paren, SearchField::any, {}};
      case ')':
        ++position_;
        return {SearchTokenKind::right_paren, SearchField::any, {}};
      case '&':
        ++position_;
        if (position_ < input_.size() && input_[position_] == '&') ++position_;
        return {SearchTokenKind::and_, SearchField::any, {}};
      case '|':
        ++position_;
        if (position_ < input_.size() && input_[position_] == '|') ++position_;
        return {SearchTokenKind::or_, SearchField::any, {}};
      case '"': {
        const auto value = read_quoted();
        if (!value || value->empty()) return {SearchTokenKind::invalid, SearchField::any, {}};
        return {SearchTokenKind::term, SearchField::any, *value};
      }
      default:
        break;
    }

    const auto start = position_;
    while (position_ < input_.size()) {
      const char character = input_[position_];
      if (std::isspace(static_cast<unsigned char>(character)) || character == '!' ||
          character == '(' || character == ')' || character == '&' || character == '|' ||
          character == '"') {
        break;
      }
      ++position_;
    }
    const std::string raw(input_.substr(start, position_ - start));
    if (equals_ascii_case_insensitive(raw, "tag:") ||
        equals_ascii_case_insensitive(raw, "path:")) {
      if (position_ >= input_.size() || input_[position_] != '"')
        return {SearchTokenKind::invalid, SearchField::any, {}};
      const auto value = read_quoted();
      if (!value || value->empty()) return {SearchTokenKind::invalid, SearchField::any, {}};
      const auto field = equals_ascii_case_insensitive(raw, "tag:")
                           ? SearchField::tag : SearchField::path;
      return {SearchTokenKind::term, field, *value};
    }
    return word_token(raw);
  }

  void next() { current_ = read_token(); }

  std::string_view input_;
  std::size_t position_ = 0;
  SearchToken current_;
};

bool expression_matches(const GraphState& state, const NodeRecord& node,
                        const SearchExpression& expression) {
  switch (expression.kind) {
    case SearchExpression::Kind::term: {
      const auto query = lower_copy(expression.value);
      if (query.empty()) return false;
      const auto partial_match = [&](std::string_view candidate) {
        return lower_copy(candidate).find(query) != std::string::npos;
      };
      const auto found = state.tags.find(node.id);
      const auto tag_match = [&] {
        if (found == state.tags.end()) return false;
        return std::ranges::any_of(found->second, partial_match);
      };
      if (expression.field == SearchField::tag) return tag_match();
      if (expression.field == SearchField::path)
        return partial_match(state.text(node.relative_path));
      return partial_match(state.text(node.name)) ||
             partial_match(state.text(node.relative_path)) || tag_match();
    }
    case SearchExpression::Kind::and_:
      return std::ranges::all_of(expression.children, [&](const auto& child) {
        return expression_matches(state, node, *child);
      });
    case SearchExpression::Kind::or_:
      return std::ranges::any_of(expression.children, [&](const auto& child) {
        return expression_matches(state, node, *child);
      });
    case SearchExpression::Kind::not_:
      return expression.children.size() == 1 &&
             !expression_matches(state, node, *expression.children.front());
  }
  return false;
}

int group_rank(const NodeRecord& node) {
  if (node.kind == NodeKind::collection || node.kind == NodeKind::work) return 0;
  switch (node.media_type) {
    case MediaType::image: return 1;
    case MediaType::video: return 2;
    case MediaType::audio: return 3;
    case MediaType::text: return 4;
    case MediaType::document: return 5;
    default: return 6;
  }
}

bool grouped_sort(const crow::request& req) {
  const auto* grouping = req.url_params.get("grouping");
  return grouping && (std::string_view(grouping) == "media_type" || std::string_view(grouping) == "grouped");
}

std::string_view filter_value(const crow::request& req) {
  const auto* value = req.url_params.get("filter");
  return value ? std::string_view(value) : std::string_view{};
}

} // namespace

bool query_flag(const crow::request& req, const char* name) {
  const auto* value = req.url_params.get(name);
  if (!value) return false;
  const std::string_view text(value);
  return text == "1" || text == "true" || text == "yes";
}

std::size_t query_size(const crow::request& req, const char* name, std::size_t fallback, std::size_t maximum) {
  const auto* value = req.url_params.get(name);
  if (!value) return fallback;
  try {
    const auto parsed = std::stoull(value);
    return std::min<std::size_t>(parsed, maximum);
  } catch (...) {
    return fallback;
  }
}

std::size_t media_cache_index(std::string_view filter) {
  if (filter == "image" || filter == "images") return 1;
  if (filter == "video" || filter == "movies") return 2;
  if (filter == "audio" || filter == "musics") return 3;
  if (filter == "text" || filter == "texts") return 4;
  if (filter == "document" || filter == "pdfs") return 5;
  return 0;
}

void sort_nodes(const GraphState& state, std::vector<NodeRef>& refs, const crow::request& req) {
  const auto* key = req.url_params.get("sort_key");
  const auto* direction = req.url_params.get("direction");
  const bool updated = key && std::string_view(key) == "updated_at";
  const bool descending = direction && std::string_view(direction) == "desc";
  const bool by_media_type = grouped_sort(req);
  std::ranges::sort(refs, [&](NodeRef a, NodeRef b) {
    const auto* na = state.get(a);
    const auto* nb = state.get(b);
    if (!na || !nb) return a < b;
    if (by_media_type && group_rank(*na) != group_rank(*nb)) {
      return group_rank(*na) < group_rank(*nb);
    }
    if (updated && na->updated_at != nb->updated_at)
      return descending ? na->updated_at > nb->updated_at : na->updated_at < nb->updated_at;
    const auto name_comparison = ordering::compare_path(state.text(na->name), state.text(nb->name));
    if (name_comparison != 0) return descending ? name_comparison > 0 : name_comparison < 0;
    const auto path_comparison = ordering::compare_path(state.text(na->relative_path), state.text(nb->relative_path));
    if (path_comparison != 0) return descending ? path_comparison > 0 : path_comparison < 0;
    return descending ? na->id > nb->id : na->id < nb->id;
  });
}

crow::json::wvalue page_json(const Manager& manager, const ReadView& view,
                             std::vector<NodeRef> refs, const crow::request& req, bool administrator,
                             bool sort, std::optional<std::size_t> forced_limit) {
  const auto filter = filter_value(req);
  if (!filter.empty() && filter != "all") {
    std::erase_if(refs, [&](NodeRef ref) { return !matches_filter(view.state(), ref, filter); });
  }
  if (sort) sort_nodes(view.state(), refs, req);
  const auto page = query_size(req, "page", 0, 1'000'000);
  const auto limit = forced_limit.value_or(query_size(req, "limit", 50, 500));
  const auto begin = std::min(page * limit, refs.size());
  const auto end = std::min(begin + limit, refs.size());
  crow::json::wvalue::list items;
  for (std::size_t i = begin; i < end; ++i)
    items.emplace_back(node_json(manager, view.state(), refs[i], administrator));
  crow::json::wvalue result;
  result["items"] = std::move(items);
  result["page"] = page;
  result["limit"] = limit;
  result["has_next"] = end < refs.size();
  result["total"] = refs.size();
  if (const auto* grouping = req.url_params.get("grouping")) result["grouping"] = std::string(grouping);
  return result;
}

std::optional<SearchQuery> parse_search_query(std::string_view query) {
  return SearchParser(query).parse();
}

bool matches_query(const GraphState& state, const NodeRecord& node, const SearchQuery& query) {
  return query.root && expression_matches(state, node, *query.root);
}

bool contains_query(const GraphState& state, const NodeRecord& node, std::string_view query) {
  const auto parsed = parse_search_query(query);
  return parsed && matches_query(state, node, *parsed);
}

bool matches_filter(const GraphState& state, NodeRef ref, std::string_view filter) {
  const auto* node = state.get(ref);
  if (!node) return false;
  if (filter.empty() || filter == "all") return true;
  if (filter == "images") filter = "image";
  else if (filter == "movies") filter = "video";
  else if (filter == "musics") filter = "audio";
  else if (filter == "texts") filter = "text";
  else if (filter == "pdfs") filter = "document";
  if (filter == "collection" || filter == "directory") return node->kind == NodeKind::collection;
  if (filter == "work") return node->kind == NodeKind::work;
  const auto matches_type = [&](MediaType type) {
    return filter == media_type_name(type);
  };
  for (const auto type : {MediaType::image, MediaType::video, MediaType::audio,
                          MediaType::text, MediaType::document}) {
    if (!matches_type(type)) continue;
    if (node->kind == NodeKind::work || node->kind == NodeKind::collection)
      return (node->media_mask & media_type_bit(type)) != 0;
    return node->media_type == type;
  }
  return false;
}

} // namespace VIEWER::api
