/**
 * This file is part of Open Chess Game Database Standard.
 *
 * Copyright (c) 2019-2026 Nguyen Hong Pham (github@nguyenpham)
 *
 * Distributed under the MIT License (MIT) (See accompanying file LICENSE.txt
 * or copy at http://opensource.org/licenses/MIT)
 */

#ifndef Parser_hpp
#define Parser_hpp

#include <vector>
#include <unordered_map>
#include <fstream>

#include "base/types.h"
#include "base/base.h"

namespace ocgdb {

    enum class ParseError {
        none,
        noinput,
        wrong_lexical,
        missing_condition,
        missing_op_comparison,
        missing_term,
        missing_factor,
        missing_close,
        invalid,
        unexpected,      // a token left over after a complete query
        unknown_word,    // a word that is neither a keyword nor a piece
        wrong_move,      // a move in move[...] or line[...] that cannot be read
        max
    };

    enum class Lex {
        none,
        string,
        number,
        fen,

        operator_begin,
        operator_and = operator_begin,
        operator_or,
        operator_not,
        operator_then,
        operator_add,
        operator_sub,
        operator_multi,
        operator_div,
        operator_comparison,
        operator_pattern_shift,

        no_node,
        comma = no_node,
        bracket,
        raw,    // the text inside move[...] or line[...], read by the parser itself
    };

    class LexWord
    {
    public:
        Lex lex = Lex::none;
        std::string string;
    };


    enum class Operator {
        op_and,
        op_or,
        op_add,
        op_sub,
        op_multi,
        op_div,
        op_eq,
        op_l,
        op_le,
        op_g,
        op_ge,
        op_ne,
        op_not,
        none
    };

    /// keyword: ply, check, mate, capture, promotion, wtm, btm (Node::keyword);
    /// move: move[...] (Node::moveSpecVec); line: line[...] (Node::lineMoves)
    enum class NodeType { none, piece, number, op, fen, pattern, keyword, move, line };

    enum class Keyword { none, ply, check, mate, capture, promotion, wtm, btm };

    /// What a query is evaluated on: the position's bitboards, and the board it came from, whose
    /// history gives the move that led to the position (`board` may be null: then move conditions
    /// are false).
    class PqlContext
    {
    public:
        const banksia::BitboardBase* bb = nullptr;
        const banksia::BoardCore* board = nullptr;
    };

    /// One move pattern of move[...]: every field left at its default matches anything.
    class MoveSpec
    {
    public:
        int pieceType = -1;
        banksia::Side side = banksia::Side::none;
        int from = -1, fromCol = -1, dest = -1, promotion = -1;
        bool capture = false;
        int castle = 0; // 1 short (O-O), 2 long (O-O-O)
    };


    class Node
    {
    public:
        Node(banksia::ChessVariant variant) : variant(variant) { }
        Node(banksia::ChessVariant variant, const LexWord& w);
        virtual ~Node();

        std::string toString() const;
        int evaluate(const PqlContext&) const;
        bool isValid() const;

        int selectSquare(const char*);
        int selectSquare(const std::string& from, const std::string& to);

        bool isInFenHashSet(uint64_t hash) const { return fenHashSet.find(hash) != fenHashSet.end(); }


    private:
        int evaluate_keyword(const PqlContext&) const;
        bool evaluate_move(const PqlContext&) const;
        bool evaluate_line(const PqlContext&) const;

        bool evaluate_pattern(const banksia::BitboardBase* bbVec0, const banksia::BitboardBase* bbVec1,
                              PatternOperand, int tolerance) const;

    public:
        NodeType nodeType = NodeType::none;
        std::string string;
        int number;
        Operator op = Operator::none;
        Node *lhs = nullptr, *rhs = nullptr;

        std::set<uint64_t> fenHashSet;

        banksia::BB* squareBB = nullptr;

        int pieceType = -1;
        banksia::Side pieceSide = banksia::Side::none;
        std::set<int> locSet;
        std::vector<banksia::BitboardBase*> patternBitBoards;

        PatternOperand patternOperand;
        int patternTolerance = 0;

        Keyword keyword = Keyword::none;
        std::vector<MoveSpec> moveSpecVec;
        std::vector<banksia::Move> lineMoves;
        uint64_t lineStartHash = 0;

        banksia::ChessVariant variant;
    };

    class Parser
    {
    public:
        Parser();
        virtual ~Parser();

        bool parse(banksia::ChessVariant, const char*);

        std::string getErrorString() const;
        void printError() const;

        /// The query's value on one position (for a `then` sequence: its first part's).
        int evaluate(const PqlContext&) const;

        /// For searching games position by position: `stage` belongs to the game being read and is 0
        /// at its start. True when the query is satisfied -- for "A then B", when a position matches
        /// B after an earlier one matched A.
        bool match(const PqlContext&, int& stage) const;
        bool isSequence() const { return stages.size() > 1; }

        void printTree() const;

    private:
        void deleteTree();
        void deleteTree(Node* node) const;

        std::vector<LexWord> lexParse(const char*);

        /// recursive descent, lowest precedence first: or, and, not, comparison, + -, * /, primary
        Node* parse_or(size_t&);
        Node* parse_and(size_t&);
        Node* parse_not(size_t&);
        Node* parse_comparison(size_t&);
        Node* parse_sum(size_t&);
        Node* parse_product(size_t&);
        Node* parse_primary(size_t&);

        Node* parse_fenclause(size_t&);
        Node* parse_fenstring(size_t&);
        Node* parse_pattern(size_t&);
        Node* parse_move(size_t&);
        Node* parse_line(size_t&);
        bool parse_movespec(const std::string&, MoveSpec&) const;

        Node* parse_piece(size_t&);
        Node* parse_piecename(size_t&);
        void parse_squareset(Node*, size_t&);

        Node* newOperator(const LexWord&, Node* lhs, Node* rhs);
        void setError(ParseError, size_t at);


        void printTree(const Node* node, std::string prefix = "") const;

        static std::string getErrorString(ParseError error);

    private:
        banksia::ChessVariant variant = banksia::ChessVariant::standard;
        banksia::BoardCore *board = nullptr, *board2 = nullptr;

        std::vector<LexWord> lexVec;
        Node* root = nullptr;           // == stages.front()
        std::vector<Node*> stages;      // the parts of "A then B then ..."

        ParseError error = ParseError::none;
        std::string errorNear;          // the token where the error was found
    };

} // namespace ocdb

#endif /* Parser_hpp */
