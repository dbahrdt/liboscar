#include <liboscar/AdvancedCellOpTree.h>

namespace liboscar {
namespace detail {
namespace AdvancedCellOpTree {
namespace parser {

Tokenizer::Tokenizer() : m_strHinter(0) {}

Tokenizer::Tokenizer(const Tokenizer::State& state) :
m_state(state),
m_strHinter(new StringHinter())
{}

Tokenizer::Tokenizer(std::string::const_iterator begin, std::string::const_iterator end) {
	m_state.it = begin;
	m_state.end = end;
}

bool Tokenizer::isScope(char c) {
	return (c == '(' || c == ')');
}


bool Tokenizer::isWhiteSpace(char c) {
	return (c == ' ' || c == '\t');
}

bool Tokenizer::isOperator(char c) {
	return (c == '+' || c == '-' || c == '/' || c == '^' || c == '%' || c == ':' || c == '<' || c == '*');
}

std::string Tokenizer::readString() {
	std::string tokenString;
	int lastValidStrSize = -1; //one passed the end == size of the valid string
	std::string::const_iterator lastValidStrIt = m_state.it;
	if (*m_state.it == '?') {
		tokenString += '?';
		++m_state.it;
		lastValidStrIt = m_state.it;
		lastValidStrSize = (int) tokenString.size();
	}
	if (*m_state.it == '"') {
		tokenString += *m_state.it;
		++m_state.it;
		while(m_state.it != m_state.end) {
			if (*m_state.it == '\\') {
				++m_state.it;
				if (m_state.it != m_state.end) {
					tokenString += *m_state.it;
					++m_state.it;
				}
				else {
					break;
				}
			}
			else if (*m_state.it == '"') {
				tokenString += *m_state.it;
				++m_state.it;
				break;
			}
			else {
				tokenString += *m_state.it;
				++m_state.it;
			}
		}
		lastValidStrIt = m_state.it;
		lastValidStrSize = (int) tokenString.size();
		if (m_state.it != m_state.end && *m_state.it == '?') {
			tokenString += *m_state.it;
			++m_state.it;
			lastValidStrIt = m_state.it;
			lastValidStrSize = (int) tokenString.size();
		}
	}
	else {
		if (lastValidStrSize > 0) { //we've read a '?'
			lastValidStrIt = m_state.it-1;
			lastValidStrSize = -1;
		}
		while (m_state.it != m_state.end) {
			if (*m_state.it == '\\') {
				++m_state.it;
				if (m_state.it != m_state.end) {
					tokenString += *m_state.it;
					++m_state.it;
				}
				else
					break;
			}
			else if (*m_state.it == ' ') {
				tokenString += *m_state.it;
				if (m_strHinter && m_strHinter->operator()(tokenString.cbegin(), tokenString.cend())) {
					if (tokenString.size() > 1 && tokenString.at(tokenString.size()-2) != ' ') {
						lastValidStrSize = (int) (tokenString.size()-1);
						lastValidStrIt = m_state.it;
					}
					++m_state.it;
				}
				else {
					tokenString.pop_back();
					break;
				}
			}
			else if (isScope(*m_state.it)) {
				//we've read a string with spaces, check if all up to here is also part of it
				if (lastValidStrSize >= 0 && tokenString.size() && tokenString.back() != ' ' && m_strHinter->operator()(tokenString.cbegin(), tokenString.cend())) {
					lastValidStrSize = (int) tokenString.size();
					lastValidStrIt = m_state.it;
				}
				break;
			}
			else if (isOperator(*m_state.it)) {
				if (tokenString.size() && tokenString.back() == ' ') {
					break;
				}
				else {
					tokenString += *m_state.it;
					++m_state.it;
				}
			}
			else {
				tokenString += *m_state.it;
				++m_state.it;
			}
		}
		if (lastValidStrSize < 0 || (m_state.it == m_state.end && m_strHinter->operator()(tokenString.cbegin(), tokenString.cend()))) {
			lastValidStrSize = (int) tokenString.size();
			lastValidStrIt = m_state.it;
		}
		else if (lastValidStrSize > 0) {
			m_state.it = lastValidStrIt;
		}
	}
	tokenString.resize(lastValidStrSize);
	return tokenString;
}

//TODO:use ragel to parse? yes, use ragel since this gets more and more complex
Token Tokenizer::next() {
	if (m_state.it == m_state.end) {
		return Token(Token::ENDOFFILE);
	}
	Token t;
	while (m_state.it != m_state.end) {
		switch (*m_state.it) {
		case '%':
		{
			t.type = Token::FM_CONVERSION_OP;
			t.value += *m_state.it;
			++m_state.it;
			//check for modifiers
			if (m_state.it != m_state.end) {
				if (*m_state.it == '#') {
					t.type = Token::REGION_DILATION_OP;
					++m_state.it;
				}
				else if('0' <= *m_state.it && '9' >= *m_state.it) {
					t.type = Token::CELL_DILATION_OP;
				}
			}
			//check if theres a number afterwards, because then this is dilation operation
			if (m_state.it != m_state.end && ('0' <= *m_state.it && '9' >= *m_state.it)) {
				bool ok = false;
				for(auto it(m_state.it); it != m_state.end; ++it) {
					if ('0' > *it || '9' < *it) {
						if (*it == '%') {
							ok = true;
							t.value.assign(m_state.it, it);
							++it;
							m_state.it = it;
						}
						break;
					}
				}
				if (!ok) {
					t.type = Token::FM_CONVERSION_OP;
				}
			}
			return t;
		}
		case '+':
		case '-':
		case '/':
		case '^':
			t.type = Token::SET_OP;
			t.value += *m_state.it;
			++m_state.it;
			return t;
		case ' ': //ignore whitespace
		case '\t':
			++m_state.it;
			break;
		case '(':
		case ')':
			t.type = *m_state.it;
			t.value += *m_state.it;
			++m_state.it;
			return t;
		case '$': //parse a region/cell/geo/path query
		{
			//read until the first occurence of :
			std::string tmp;
			for(++m_state.it; m_state.it != m_state.end;) {
				if (*m_state.it == ':') {
					++m_state.it;
					break;
				}
				else {
					tmp += *m_state.it;
					++m_state.it;
				}
			}
			bool opSeparates = false;
			if (tmp == "region") {
				t.type = Token::REGION;
				opSeparates = true;
			}
			else if (tmp == "rec") {
				t.type = Token::REGION_EXCLUSIVE_CELLS;
				opSeparates = true;
			}
			else if (tmp == "qec") {
				t.type = Token::QUERY_EXCLUSIVE_CELLS;
			}
			else if (tmp == "cell") {
				t.type = Token::CELL;
				opSeparates = true;
			}
			else if (tmp == "cells") {
				t.type = Token::CELLS;
			}
			else if (tmp == "triangle") {
				t.type = Token::TRIANGLE;
			}
			else if (tmp == "triangles") {
				t.type = Token::TRIANGLES;
			}
			else if (tmp  == "item") {
				t.type = Token::ITEM;
				opSeparates = true;
			}
			else if (tmp == "geo") {
				t.type = Token::GEO_RECT;
			}
			else if (tmp == "poly") {
				t.type = Token::GEO_POLYGON;
			}
			else if (tmp == "path") {
				t.type = Token::GEO_PATH;
			}
			else if (tmp == "point") {
				t.type = Token::GEO_POINT;
			}
			for(; m_state.it != m_state.end;) {
				if (isWhiteSpace(*m_state.it) || (opSeparates && isOperator(*m_state.it)) || isScope(*m_state.it)) {
					break;
				}
				else {
					t.value += *m_state.it;
					++m_state.it;
				}
			}
			return t;
			break;
		}
		case ':':
		{
			t.type = Token::COMPASS_OP;
			++m_state.it;
			for(auto it(m_state.it); it != m_state.end; ++it) {
				if (*it == ' ' || *it == '\t' || *it == '(') {
					t.value.assign(m_state.it, it);
					m_state.it = it;
					break;
				}
			}
			return t;
		}
		case '*':
		{
			t.type = Token::RELEVANT_ELEMENT_OP;
			++m_state.it;
			t.value = "*";
			return t;
		}
		case '<':
		{
			t.type = Token::BETWEEN_OP;
			const char * cmp = "<->";
			auto it(m_state.it);
			for(int i(0); it != m_state.end && i < 3 && *it == *cmp; ++it, ++cmp, ++i) {}
			t.value.assign(m_state.it, it);
			m_state.it = it;
			return t;
		}
		case '#':
		{
			t.type = Token::STRING_REGION;
			++m_state.it;
			t.value = readString();
			return t;
		}
		case '!':
		{
			t.type = Token::STRING_ITEM;
			++m_state.it;
			t.value = readString();
			return t;
		}
		default: //read as normal string
		{
			t.type = Token::STRING;
			t.value = readString();
			return t;
		}
		};
	}
	if (m_state.it == m_state.end) {
		t.type = Token::ENDOFFILE;
	}
	return t;
}

bool Parser::pop() {
	if (-1 == m_lastToken.type) {
		throw std::runtime_error("pop() without peek()");
	}
	m_prevToken = m_lastToken;
	// stop at EOF, otherwise consume it
	if (Token::ENDOFFILE != m_lastToken.type) {
		m_lastToken.type = -1;
	}
	return true;
}

//dont turn return type to reference or const reference since this function may be called while tehre ist still a reference on the return value
Token Parser::peek() {
	if (-1 == m_lastToken.type) {
		m_lastToken = m_tokenizer.next();
	}
	return m_lastToken;
}

bool Parser::eat(Token::Type t) {
	if (t != peek().type) {
		return false;
	}
	pop();
	return true;
}

detail::AdvancedCellOpTree::Node* Parser::parseUnaryOps() {
	Token t = peek();
	int nst = 0;
	switch (t.type) {
	case Token::CELL_DILATION_OP:
		nst = Node::CELL_DILATION_OP;
		break;
	case Token::REGION_DILATION_OP:
		nst = Node::REGION_DILATION_OP;
		break;
	case Token::FM_CONVERSION_OP:
		nst = Node::FM_CONVERSION_OP;
		break;
	case Token::COMPASS_OP:
		nst = Node::COMPASS_OP;
		break;
	case Token::RELEVANT_ELEMENT_OP:
		nst = Node::RELEVANT_ELEMENT_OP;
		break;
	case Token::QUERY_EXCLUSIVE_CELLS:
		nst = Node::QUERY_EXCLUSIVE_CELLS;
		break;
	default:
		break;
	}
	
	switch (t.type) {
	case Token::FM_CONVERSION_OP:
	case Token::CELL_DILATION_OP:
	case Token::REGION_DILATION_OP:
	case Token::COMPASS_OP:
	case Token::RELEVANT_ELEMENT_OP:
	case Token::QUERY_EXCLUSIVE_CELLS:
	{
		pop();
		Node * unaryOpNode = new Node();
		unaryOpNode->baseType = Node::UNARY_OP;
		unaryOpNode->subType = nst;
		unaryOpNode->value = t.value;
		Node* cn = parseSingleQ();
		if (cn) {
			unaryOpNode->children.push_back(cn);
			return unaryOpNode;
		}
		else { //something went wrong, skip this op
			return 0;
		}
		break;
	}
	default://hand back
		return 0;
	};
	SSERIALIZE_CHEAP_ASSERT(false);
	return 0;
}

//parses a Single query like STRING, REGION, CELL, GEO_RECT, GEO_PATH
//calls parseQ() on opening a new SCOPE
detail::AdvancedCellOpTree::Node* Parser::parseSingleQ() {
	Token t = peek();
	switch (t.type) {
	case '(': //opens a new query
	{
		pop();
		Node * tmp = parseQ();
		eat((Token::Type)')');
		return tmp;
	}
	case Token::QUERY_EXCLUSIVE_CELLS:
	case Token::FM_CONVERSION_OP:
	case Token::CELL_DILATION_OP:
	case Token::REGION_DILATION_OP:
	case Token::COMPASS_OP:
	case Token::RELEVANT_ELEMENT_OP:
	{
		return parseUnaryOps();
	}
	case Token::CELL:
	{
		pop();
		return new Node(Node::LEAF, Node::CELL, t.value);
	}
	case Token::CELLS:
	{
		pop();
		return new Node(Node::LEAF, Node::CELLS, t.value);
	}
	case Token::TRIANGLE:
	{
		pop();
		return new Node(Node::LEAF, Node::TRIANGLE, t.value);
	}
	case Token::TRIANGLES:
	{
		pop();
		return new Node(Node::LEAF, Node::TRIANGLES, t.value);
	}
	case Token::REGION:
	{
		pop();
		return new Node(Node::LEAF, Node::REGION, t.value);
		break;
	}
	case Token::REGION_EXCLUSIVE_CELLS:
	{
		pop();
		return new Node(Node::LEAF, Node::REGION_EXCLUSIVE_CELLS, t.value);
		break;
	}
	case Token::GEO_PATH:
	{
		pop();
		return new Node(Node::LEAF, Node::PATH, t.value);
		break;
	}
	case Token::GEO_POINT:
	{
		pop();
		return new Node(Node::LEAF, Node::POINT, t.value);
		break;
	}
	case Token::GEO_RECT:
	{
		pop();
		return new Node(Node::LEAF, Node::RECT, t.value);
		break;
	}
	case Token::GEO_POLYGON:
	{
		pop();
		return new Node(Node::LEAF, Node::POLYGON, t.value);
		break;
	}
	case Token::STRING:
	{
		pop();
		return new Node(Node::LEAF, Node::STRING, t.value);
		break;
	}
	case Token::STRING_ITEM:
	{
		pop();
		return new Node(Node::LEAF, Node::STRING_ITEM, t.value);
		break;
	}
	case Token::STRING_REGION:
	{
		pop();
		return new Node(Node::LEAF, Node::STRING_REGION, t.value);
		break;
	}
	case Token::ITEM:
	{
		pop();
		return new Node(Node::LEAF, Node::ITEM, t.value);
		break;
	}
	case Token::ENDOFFILE:
	default: //hand back to caller, somethings wrong
		return 0;
	};
}


//(((())) ggh)
//Q ++ Q
detail::AdvancedCellOpTree::Node* Parser::parseQ() {
	Node * n = 0;
	for(;;) {
		Token t = peek();
		Node * curTokenNode = 0;
		switch (t.type) {
		case ')': //leave scope, caller removes closing brace
			//check if there's an unfinished operation, if there is ditch it
			if (n && n->baseType == Node::BINARY_OP && n->children.size() == 1) {
				Node* tmp = n;
				n = tmp->children.front();
				tmp->children.clear();
				delete tmp;
			}
			return n;
			break;
		case Token::FM_CONVERSION_OP:
		case Token::CELL_DILATION_OP:
		case Token::REGION_DILATION_OP:
		case Token::COMPASS_OP:
		case Token::RELEVANT_ELEMENT_OP:
		case Token::QUERY_EXCLUSIVE_CELLS:
		{
			curTokenNode = parseUnaryOps();
			if (!curTokenNode) { //something went wrong, skip this op
				continue;
			}
			break;
		}
		case Token::SET_OP:
		case Token::BETWEEN_OP:
		{
			pop();
			 //we need to have a valid child, otherwise this operation is bogus (i.e. Q ++ Q)
			if (n && !(n->baseType == Node::BINARY_OP && n->children.size() < 2)) {
				Node * opNode = new Node();
				opNode->baseType = Node::BINARY_OP;
				opNode->subType = (t.type == Token::SET_OP ? Node::SET_OP : Node::BETWEEN_OP);
				opNode->value = t.value;
				opNode->children.push_back(n);
				n = 0;
				//get the other child in the next round
				curTokenNode = opNode;
			}
			else { //skip this op
				continue;
			}
			break;
		}
		case Token::ENDOFFILE:
			return n;
			break;
		case Token::INVALID_TOKEN:
		case Token::INVALID_CHAR:
			pop();
			break;
		default:
			curTokenNode = parseSingleQ();
			break;
		};
		if (!curTokenNode) {
			continue;
		}
		if (n) {
			if (n->baseType == Node::BINARY_OP && n->children.size() < 2) {
				n->children.push_back(curTokenNode);
			}
			else {//implicit intersection
				Node * opNode = new Node(Node::BINARY_OP, Node::SET_OP, " ");
				opNode->children.push_back(n);
				opNode->children.push_back(curTokenNode);
				n = opNode;
			}
		}
		else {
			n = curTokenNode;
		}
	}
	//this should never happen
	SSERIALIZE_CHEAP_ASSERT(false);
	return n;
}

Parser::Parser() {
	m_prevToken.type = -1;
	m_lastToken.type = -1;
}

detail::AdvancedCellOpTree::Node* Parser::parse(const std::string & str) {
	m_str.clear();
	//sanitize string, add as many openeing braces at the beginning as neccarray or as manny closing braces at the end
	{
		int obCount = 0;
		for(char c : str) {
			if (c == '(') {
				++obCount;
				m_str += c;
			}
			else if (c == ')') {
				if (obCount > 0) {
					m_str += c;
					--obCount;
				}
				//else not enough opening braces, so skip this one
			}
			else {
				m_str += c;
			}
		}
		//add remaining closing braces to get a well-formed query
		for(; obCount > 0;) {
			m_str += ')';
			--obCount;
		}
	}
	m_tokenizer  = Tokenizer(m_str.begin(), m_str.end());
	m_lastToken.type = -1;
	m_prevToken.type = -1;
	Node* n = parseQ();
	return n;
}

}}}//end namespace detail::AdvancedCellOpTree::parser

AdvancedCellOpTree::
AdvancedCellOpTree(
	const sserialize::Static::CellTextCompleter & ctc,
	const sserialize::Static::CQRDilator & cqrd,
	const CQRFromComplexSpatialQuery & csq,
	const sserialize::spatial::GeoHierarchySubGraph & ghsg) :
m_ctc(ctc),
m_cqrd(cqrd),
m_csq(csq),
m_ghsg(ghsg),
m_root(0)
{}

AdvancedCellOpTree::~AdvancedCellOpTree() {
	if (m_root) {
		delete m_root;
	}
}

sserialize::CellQueryResult AdvancedCellOpTree::CalcBase::calcBetweenOp(const sserialize::CellQueryResult& c1, const sserialize::CellQueryResult& c2) {
#ifdef SSERIALIZE_EXPENSIVE_ASSERT_ENABLED
	auto tmp = m_csq.betweenOp(c1, c2, 1);
	for(const sserialize::ItemIndex & x : tmp) {
		SSERIALIZE_EXPENSIVE_ASSERT(std::is_sorted(x.begin(), x.end()));
		SSERIALIZE_EXPENSIVE_ASSERT(sserialize::is_strong_monotone_ascending(x.begin(), x.end()));
		for(uint32_t y : x) {
			SSERIALIZE_EXPENSIVE_ASSERT_NOT_EQUAL(y, uint32_t(16800296));
		}
	}
	sserialize::ItemIndex tmp2 =  tmp.flaten();
	for(uint32_t y : tmp2) {
		SSERIALIZE_EXPENSIVE_ASSERT_NOT_EQUAL(y, uint32_t(16800296));
	}
	return tmp;
#else
	return m_csq.betweenOp(c1, c2, m_threadCount);
#endif
}

sserialize::CellQueryResult AdvancedCellOpTree::CalcBase::calcCompassOp(liboscar::AdvancedCellOpTree::Node* node, const sserialize::CellQueryResult& cqr) {
	CQRFromComplexSpatialQuery::UnaryOp direction = CQRFromComplexSpatialQuery::UO_INVALID;
	if (node->value == "^" || node->value == "north-of") {
		direction = CQRFromComplexSpatialQuery::UO_NORTH_OF;
	}
	else if (node->value == ">" || node->value == "east-of") {
		direction = CQRFromComplexSpatialQuery::UO_EAST_OF;
	}
	else if (node->value == "v" || node->value == "south-of") {
		direction = CQRFromComplexSpatialQuery::UO_SOUTH_OF;
	}
	else if (node->value == "<" || node->value == "west-of") {
		direction = CQRFromComplexSpatialQuery::UO_WEST_OF;
	}
	return m_csq.compassOp(cqr, direction, m_threadCount);
}

sserialize::CellQueryResult AdvancedCellOpTree::CalcBase::calcRelevantElementOp(liboscar::AdvancedCellOpTree::Node* node, const sserialize::CellQueryResult& cqr) {
	assert(node && node->value == "*");
	return m_csq.relevantElementOp(cqr);
}

sserialize::ItemIndex AdvancedCellOpTree::CalcBase::calcDilateRegionOp(AdvancedCellOpTree::Node * node, const sserialize::CellQueryResult & cqr) {
	const sserialize::Static::spatial::GeoHierarchy & gh = m_ctc.geoHierarchy();
	const sserialize::Static::ItemIndexStore & idxStore = this->idxStore();

	double th = sserialize::stod(node->value.c_str());
	if (th <= 0.0) {
		return sserialize::ItemIndex();
	}
	th /= 100.0;
	
	std::vector<uint32_t> regions;

	std::unordered_map<uint32_t, uint32_t> r2cc;
	
	for(auto it(cqr.cbegin()), end(cqr.cend()); it != end; ++it) {
		uint32_t cellId = it.cellId();
		for(uint32_t cP(gh.cellParentsBegin(cellId)), cE(gh.cellParentsEnd(cellId)); cP != cE; ++cP) {
			uint32_t rId = gh.cellPtr(cP);
			if (r2cc.count(rId)) {
				r2cc[rId] += 1;
			}
			else {
				r2cc[rId] = 1;
			}
		}
	}
	for(auto x : r2cc) {
		if ( (double)x.second / (double)idxStore.idxSize( gh.regionCellIdxPtr(x.first) ) > th) {
			regions.push_back(x.first);
		}
	}
	
	//regions need to be unique
	sserialize::ItemIndex res = sserialize::treeReduceMap<std::vector<uint32_t>::iterator, sserialize::ItemIndex>(regions.begin(), regions.end(),
		[](const sserialize::ItemIndex & a, const sserialize::ItemIndex & b) { return a+b; },
		[&gh, &idxStore](uint32_t x) { return idxStore.at( gh.regionCellIdxPtr(x) ); }
	);
	return res;
}

std::vector<double> AdvancedCellOpTree::CalcBase::asDoubles(const std::string & str) {
	std::vector<double> tmp;
	{
		struct MyOut {
			std::vector<double> * dest;
			MyOut & operator++() { return *this; }
			MyOut & operator*() { return *this; }
			MyOut & operator=(const std::string & str) {
				try {
					double t = sserialize::stod(str);
					dest->push_back(t);
				}
				catch (std::invalid_argument & e) {}
				return *this;
			}
			MyOut(std::vector<double> * d) : dest(d) {}
		};
		typedef sserialize::OneValueSet<uint32_t> MyS;
		sserialize::split<std::string::const_iterator, MyS, MyS, MyOut>(str.begin(), str.end(), MyS(','), MyS('\\'), MyOut(&tmp));
	}
	return tmp;
}

const sserialize::Static::spatial::GeoHierarchy& AdvancedCellOpTree::CalcBase::gh() const {
	return m_ctc.geoHierarchy();
}

const sserialize::Static::ItemIndexStore& AdvancedCellOpTree::CalcBase::idxStore() const {
	return m_ctc.idxStore();
}

const liboscar::Static::OsmKeyValueObjectStore & AdvancedCellOpTree::CalcBase::store() const {
	return m_csq.cqrfp().store();
}

const sserialize::spatial::GeoHierarchySubGraph & AdvancedCellOpTree::CalcBase::ghsg() const {
	return m_ghsg;
}

sserialize::Static::CellTextCompleter & AdvancedCellOpTree::CalcBase::ctc() {
		return m_ctc;
}

uint32_t AdvancedCellOpTree::CalcBase::threadCount() const {
	return m_threadCount;
}

sserialize::CellQueryResult AdvancedCellOpTree::CalcBase::toCQR(const sserialize::TreedCellQueryResult & cqr) const {
	return cqr.toCQR( this->threadCount() );
}

void AdvancedCellOpTree::parse(const std::string& str) {
	if (m_root) {
		delete m_root;
		m_root = 0;
	}
	detail::AdvancedCellOpTree::parser::Parser p;
	m_root = p.parse(str);
}

template<>
sserialize::CellQueryResult
AdvancedCellOpTree::Calc<sserialize::CellQueryResult>::calcDilationOp(AdvancedCellOpTree::Node* node) {
	double diameter = sserialize::stod(node->value.c_str())*1000;
	sserialize::CellQueryResult cqr( calc(node->children.front()) );
	return sserialize::CellQueryResult( m_cqrd.dilate(cqr, diameter, m_threadCount), cqr.geoHierarchy(), cqr.idxStore(), sserialize::CellQueryResult::FF_CELL_GLOBAL_ITEM_IDS) + cqr;
}

template<>
sserialize::TreedCellQueryResult
AdvancedCellOpTree::Calc<sserialize::TreedCellQueryResult>::calcDilationOp(AdvancedCellOpTree::Node* node) {
	double diameter = sserialize::stod(node->value.c_str())*1000;
	sserialize::TreedCellQueryResult cqr( calc(node->children.front()) );
	return sserialize::TreedCellQueryResult( m_cqrd.dilate(toCQR(cqr), diameter, m_threadCount), cqr.geoHierarchy(), cqr.idxStore() ) + cqr;
}


template<>
sserialize::CellQueryResult
AdvancedCellOpTree::Calc<sserialize::CellQueryResult>::calcRegionDilationOp(AdvancedCellOpTree::Node* node) {
	sserialize::CellQueryResult cqr( calc(node->children.front()) );
	return sserialize::CellQueryResult( CalcBase::calcDilateRegionOp(node, cqr) , cqr.geoHierarchy(), cqr.idxStore(), sserialize::CellQueryResult::FF_CELL_GLOBAL_ITEM_IDS);
}

template<>
sserialize::TreedCellQueryResult
AdvancedCellOpTree::Calc<sserialize::TreedCellQueryResult>::calcRegionDilationOp(AdvancedCellOpTree::Node* node) {
	sserialize::TreedCellQueryResult cqr( calc(node->children.front()) );
	return sserialize::TreedCellQueryResult( CalcBase::calcDilateRegionOp(node, toCQR(cqr)), cqr.geoHierarchy(), cqr.idxStore() );
}

template<>
sserialize::CellQueryResult
AdvancedCellOpTree::Calc<sserialize::CellQueryResult>::calcBetweenOp(AdvancedCellOpTree::Node* node) {
	SSERIALIZE_CHEAP_ASSERT(node->children.size() == 2);
	return CalcBase::calcBetweenOp(calc(node->children.front()), calc(node->children.back()));
}

template<>
sserialize::TreedCellQueryResult
AdvancedCellOpTree::Calc<sserialize::TreedCellQueryResult>::calcBetweenOp(AdvancedCellOpTree::Node* node) {
	SSERIALIZE_CHEAP_ASSERT(node->children.size() == 2);
	return sserialize::TreedCellQueryResult( CalcBase::calcBetweenOp(toCQR(calc(node->children.front())), toCQR(calc(node->children.back()))) );
}

template<>
sserialize::CellQueryResult
AdvancedCellOpTree::Calc<sserialize::CellQueryResult>::calcCompassOp(AdvancedCellOpTree::Node* node) {
	SSERIALIZE_CHEAP_ASSERT(node->children.size() == 1);
	return CalcBase::calcCompassOp(node, calc(node->children.front()));
}

template<>
sserialize::TreedCellQueryResult
AdvancedCellOpTree::Calc<sserialize::TreedCellQueryResult>::calcCompassOp(AdvancedCellOpTree::Node* node) {
	SSERIALIZE_CHEAP_ASSERT(node->children.size() == 1);
	return sserialize::TreedCellQueryResult( CalcBase::calcCompassOp(node, toCQR(calc(node->children.front()))) );
}

template<>
sserialize::CellQueryResult
AdvancedCellOpTree::Calc<sserialize::CellQueryResult>::calcRelevantElementOp(AdvancedCellOpTree::Node* node) {
	SSERIALIZE_CHEAP_ASSERT(node->children.size() == 1);
	return CalcBase::calcRelevantElementOp(node, calc(node->children.front()));
}

template<>
sserialize::TreedCellQueryResult
AdvancedCellOpTree::Calc<sserialize::TreedCellQueryResult>::calcRelevantElementOp(AdvancedCellOpTree::Node* node) {
	SSERIALIZE_CHEAP_ASSERT(node->children.size() == 1);
	return sserialize::TreedCellQueryResult( CalcBase::calcRelevantElementOp(node, toCQR(calc(node->children.front()))) );
}


}//end namespace
