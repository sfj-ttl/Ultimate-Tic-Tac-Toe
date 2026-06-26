#include <algorithm>
#include <limits>
#include <random>
#include <sstream>
#include <string>
#include <vector>

using namespace std;

enum class Mark { Empty, X, O, Draw };

struct Position {
    int row = -1;
    int col = -1;
};

class board33 {
public:
    board33() {
        reset();
    }

    void reset() {
        Smallboard = vector<vector<Mark>>(3, vector<Mark>(3, Mark::Empty));
        owner = Mark::Empty;
        numofempty = 9;
    }

    bool place(int row, int col, Mark player) {
        if (row < 0 || row >= 3 || col < 0 || col >= 3) return false;
        if (Smallboard[row][col] == Mark::Empty) {
            Smallboard[row][col] = player;
            numofempty--;
            updateowner();
            return true;
        }
        return false;
    }

    bool isopen() const {
        return numofempty > 0;
    }

    bool iswon() const {
        return owner == Mark::X || owner == Mark::O;
    }

    Mark getowner() const {
        return owner;
    }

    Mark getmark(int row, int col) const {
        if (row < 0 || row >= 3 || col < 0 || col >= 3) return Mark::Empty;
        return Smallboard[row][col];
    }

private:
    vector<vector<Mark>> Smallboard = vector<vector<Mark>>(3, vector<Mark>(3, Mark::Empty));
    Mark owner = Mark::Empty;
    int numofempty = 9;

    bool isfull() const {
        return numofempty == 0;
    }

    void updateowner() {
        if (owner != Mark::Empty) return;

        Mark won = winner();
        if (won != Mark::Empty) {
            owner = won;
            return;
        }
        if (isfull()) {
            owner = Mark::Draw;
        }
    }

    Mark winner() const {
        for (int i = 0; i < 3; i++) {
            if (Smallboard[i][0] != Mark::Empty && Smallboard[i][0] == Smallboard[i][1] && Smallboard[i][1] == Smallboard[i][2]) {
                return Smallboard[i][0];
            }
        }
        for (int i = 0; i < 3; i++) {
            if (Smallboard[0][i] != Mark::Empty && Smallboard[0][i] == Smallboard[1][i] && Smallboard[0][i] == Smallboard[2][i]) {
                return Smallboard[0][i];
            }
        }
        if (Smallboard[0][0] != Mark::Empty && Smallboard[0][0] == Smallboard[1][1] && Smallboard[0][0] == Smallboard[2][2]) {
            return Smallboard[0][0];
        }
        if (Smallboard[0][2] != Mark::Empty && Smallboard[0][2] == Smallboard[1][1] && Smallboard[0][2] == Smallboard[2][0]) {
            return Smallboard[0][2];
        }
        return Mark::Empty;
    }
};

class game {
public:
    game() {
        reset();
    }

    void reset() {
        board = vector<vector<board33>>(3, vector<board33>(3, board33()));
        bigboard = board33();
        gameplayer = Mark::X;
        winner = Mark::Empty;
        forcerow = -1;
        forcecol = -1;
        force_condition = false;
        over = false;
        lastrow = -1;
        lastcol = -1;
        xnum=0;
        onum=0;
        message = "X starts";
    }

    bool makemove(int row, int col) {
        string err;//对错误输入的提示字符
        return makemove(row, col, err);
    }

    bool makemove(int row, int col, string& err) {
        if (over) {
            err = "Game is already over";
            return false;
        }
        if (row < 0 || row >= 9 || col < 0 || col >= 9) {
            err = "Move is outside the board";
            return false;
        }

        if (forcerow >= 0 && forcecol >= 0) force_condition_change(forcerow, forcecol);
        int big_row = row / 3;
        int big_col = col / 3;
        if ((big_row != forcerow || big_col != forcecol) && force_condition) {
            err = "You must play in the forced big board";
            return false;
        }
        if (!board[big_row][big_col].isopen()) {
            err = "That big board is closed";
            return false;
        }

        int small_row = row - 3 * big_row;
        int small_col = col - 3 * big_col;
        bool placed = board[big_row][big_col].place(small_row, small_col, player());//落子
        if (!placed) {
            err = "That cell is already occupied";
            return false;
        }
        lastrow = row;
        lastcol = col;//记录上一步的位置

        if (board[big_row][big_col].iswon()) {
            bigboard.place(big_row, big_col, player());
            if(player()==Mark::X) xnum++;
            if(player()==Mark::O) onum++;
            if (bigboard.iswon()) {
                over = true;
                winner = player();
                forcerow = -1;
                forcecol = -1;
                force_condition = false;
                message = string(1, markchar(winner)) + " wins";
                return true;
            }
        }

        if (allclosed()) {
            over = true;
            if(xnum==onum){
                winner = Mark::Draw;
                message = "Draw";
            }
            else if(xnum>onum){
                winner = Mark::X;
                message = "X wins";
            }
            else if(xnum<onum){
                winner = Mark::X;
                message = "O wins";
            }
            
            forcerow = -1;
            forcecol = -1;
            force_condition = false;
            return true;
        }

        switchplayer();
        forcerow = small_row;
        forcecol = small_col;
        force_condition_change(forcerow, forcecol);
        if (!force_condition) {
            forcerow = -1;
            forcecol = -1;
        }
        message = force_condition
            ? string(1, markchar(gameplayer)) + " to move: play in big board row " + to_string(forcerow + 1) + ", col " + to_string(forcecol + 1)
            : string(1, markchar(gameplayer)) + " to move: play in any open big board";
        return true;
    }

    string tojson() const {
        ostringstream out;
        out << "{\"ok\":true,\"board\":[";
        for (int row = 0; row < 9; row++) {
            if (row) out << ",";
            out << "[";
            for (int col = 0; col < 9; col++) {
                if (col) out << ",";
                out << "\"" << markchar(getcell(row, col)) << "\"";
            }
            out << "]";
        }

        out << "],\"smallBoards\":[";
        for (int row = 0; row < 3; row++) {
            if (row) out << ",";
            out << "[";
            for (int col = 0; col < 3; col++) {
                if (col) out << ",";
                out << "\"" << markchar(board[row][col].getowner()) << "\"";
            }
            out << "]";
        }

        out << "],\"currentPlayer\":\"" << markchar(gameplayer)
            << "\",\"forcedBigRow\":" << forcerow
            << ",\"forcedBigCol\":" << forcecol
            << ",\"lastMoveRow\":" << lastrow
            << ",\"lastMoveCol\":" << lastcol
            << ",\"winner\":\"" << markchar(winner)
            << "\",\"gameOver\":" << (over ? "true" : "false")
            << ",\"message\":\"" << escapejson(message)
            << "\",\"legalMoves\":[";

        vector<Position> moves = getlegalmoves();
        for (size_t i = 0; i < moves.size(); i++) {
            if (i) out << ",";
            out << "{\"row\":" << moves[i].row << ",\"col\":" << moves[i].col << "}";
        }
        out << "]}";
        return out.str();
    }

    string errorjson(const string& msg) const {
        return string("{\"ok\":false,\"message\":\"") + escapejson(msg) + "\"}";
    }

    bool isgameover() const {
        return over;
    }

    Mark player() const {
        return gameplayer;
    }

    bool makeaimove(int level, string& err) {
        vector<Position> moves = getlegalmoves();
        if (over) {
            err = "Game is already over";
            return false;
        }
        if (moves.empty()) {
            err = "No legal moves";
            return false;
        }

        Position move = chooseaimove(level);
        return makemove(move.row, move.col, err);
    }

private:
    vector<vector<board33>> board = vector<vector<board33>>(3, vector<board33>(3, board33()));
    board33 bigboard = board33();
    Mark gameplayer = Mark::X;
    Mark winner = Mark::Empty;
    int forcerow = -1;
    int forcecol = -1;
    int lastrow = -1;
    int lastcol = -1;
    bool force_condition = false;
    bool over = false;
    string message = "X starts";
    int xnum=0;
    int onum=0;

    Mark getcell(int row, int col) const {
        return board[row / 3][col / 3].getmark(row % 3, col % 3);
    }

    bool canmove(int row, int col) const {
        if (over || row < 0 || row >= 9 || col < 0 || col >= 9) return false;
        int big_row = row / 3;
        int big_col = col / 3;
        if (force_condition && (big_row != forcerow || big_col != forcecol)) return false;
        if (!board[big_row][big_col].isopen()) return false;
        return getcell(row, col) == Mark::Empty;
    }

    vector<Position> getlegalmoves() const {
        vector<Position> moves;
        for (int row = 0; row < 9; row++) {
            for (int col = 0; col < 9; col++) {
                if (canmove(row, col)) moves.push_back({row, col});
            }
        }
        return moves;
    }

    bool allclosed() const {
        for (int row = 0; row < 3; row++) {
            for (int col = 0; col < 3; col++) {
                if (board[row][col].isopen()) return false;
            }
        }
        return true;
    }

    void switchplayer() {
        if (gameplayer == Mark::X) {
            gameplayer = Mark::O;
        } else {
            gameplayer = Mark::X;
        }
    }

    void force_condition_change(int row, int col) {
        force_condition = board[row][col].isopen();
    }

    static char markchar(Mark mark) {
        if (mark == Mark::X) return 'X';
        if (mark == Mark::O) return 'O';
        if (mark == Mark::Draw) return 'D';
        return '.';
    }

    static Mark opposite(Mark mark) {
        return mark == Mark::X ? Mark::O : Mark::X;
    }

    static string escapejson(const string& text) {
        string escaped;
        for (char ch : text) {
            if (ch == '\\' || ch == '"') escaped += '\\';
            escaped += ch;
        }
        return escaped;
    }

    Position chooseaimove(int level) const {
        vector<Position> moves = getlegalmoves();
        static random_device rd;
        static mt19937 rng(rd());
        shuffle(moves.begin(), moves.end(), rng);

        if (level <= 1) {
            return moves.front();
        }

        Mark ai = gameplayer;
        int depth = level >= 3 ? 3 : 1;
        int bestScore = numeric_limits<int>::min();
        vector<Position> bestMoves;

        for (const Position& move : moves) {
            game next = *this;
            string ignored;
            next.makemove(move.row, move.col, ignored);
            int score = level >= 3
                ? next.minimax(depth - 1, numeric_limits<int>::min() / 2, numeric_limits<int>::max() / 2, ai)
                : next.evaluate(ai);

            if (score > bestScore) {
                bestScore = score;
                bestMoves.clear();
                bestMoves.push_back(move);
            } else if (score == bestScore) {
                bestMoves.push_back(move);
            }
        }

        uniform_int_distribution<int> pick(0, static_cast<int>(bestMoves.size()) - 1);
        return bestMoves[pick(rng)];
    }

    int minimax(int depth, int alpha, int beta, Mark ai) const {
        if (depth <= 0 || over) return evaluate(ai);

        vector<Position> moves = getlegalmoves();
        if (moves.empty()) return evaluate(ai);

        sort(moves.begin(), moves.end(), [&](const Position& a, const Position& b) {
            game nextA = *this;
            game nextB = *this;
            string ignored;
            nextA.makemove(a.row, a.col, ignored);
            nextB.makemove(b.row, b.col, ignored);
            int scoreA = nextA.evaluate(ai);
            int scoreB = nextB.evaluate(ai);
            return gameplayer == ai ? scoreA > scoreB : scoreA < scoreB;
        });

        if (gameplayer == ai) {
            int best = numeric_limits<int>::min() / 2;
            for (const Position& move : moves) {
                game next = *this;
                string ignored;
                next.makemove(move.row, move.col, ignored);
                best = max(best, next.minimax(depth - 1, alpha, beta, ai));
                alpha = max(alpha, best);
                if (beta <= alpha) break;
            }
            return best;
        }

        int best = numeric_limits<int>::max() / 2;
        for (const Position& move : moves) {
            game next = *this;
            string ignored;
            next.makemove(move.row, move.col, ignored);
            best = min(best, next.minimax(depth - 1, alpha, beta, ai));
            beta = min(beta, best);
            if (beta <= alpha) break;
        }
        return best;
    }

    int evaluate(Mark ai) const {
        Mark opponent = opposite(ai);
        if (over) {
            if (winner == ai) return 1000000;
            if (winner == opponent) return -1000000;
            return 0;
        }

        int score = 0;
        Mark owners[3][3];
        for (int row = 0; row < 3; row++) {
            for (int col = 0; col < 3; col++) {
                owners[row][col] = board[row][col].getowner();
                if (owners[row][col] == ai) score += 3000 + macropositionbonus(row, col);
                else if (owners[row][col] == opponent) score -= 3000 + macropositionbonus(row, col);
                else if (owners[row][col] == Mark::Empty) score += evaluatesmallboard(row, col, ai);
            }
        }

        score += evaluategrid(owners, ai, 9000);

        if (force_condition && forcerow >= 0 && forcecol >= 0) {
            Mark forcedOwner = board[forcerow][forcecol].getowner();
            if (forcedOwner == Mark::Empty) score += board[forcerow][forcecol].isopen() ? 12 : -12;
        }

        return score;
    }

    int evaluatesmallboard(int bigRow, int bigCol, Mark ai) const {
        Mark cells[3][3];
        for (int row = 0; row < 3; row++) {
            for (int col = 0; col < 3; col++) {
                cells[row][col] = board[bigRow][bigCol].getmark(row, col);
            }
        }

        int score = evaluategrid(cells, ai, 120);
        if (cells[1][1] == ai) score += 14;
        else if (cells[1][1] == opposite(ai)) score -= 14;

        const int corners[4][2] = {{0, 0}, {0, 2}, {2, 0}, {2, 2}};
        for (const auto& corner : corners) {
            Mark mark = cells[corner[0]][corner[1]];
            if (mark == ai) score += 5;
            else if (mark == opposite(ai)) score -= 5;
        }
        return score;
    }

    int evaluategrid(Mark cells[3][3], Mark ai, int scale) const {
        int score = 0;
        for (int i = 0; i < 3; i++) {
            score += evaluateline(cells[i][0], cells[i][1], cells[i][2], ai, scale);
            score += evaluateline(cells[0][i], cells[1][i], cells[2][i], ai, scale);
        }
        score += evaluateline(cells[0][0], cells[1][1], cells[2][2], ai, scale);
        score += evaluateline(cells[0][2], cells[1][1], cells[2][0], ai, scale);
        return score;
    }

    int evaluateline(Mark a, Mark b, Mark c, Mark ai, int scale) const {
        Mark opponent = opposite(ai);
        Mark cells[3] = {a, b, c};
        int aiCount = 0;
        int opponentCount = 0;
        int emptyCount = 0;

        for (Mark mark : cells) {
            if (mark == ai) aiCount++;
            else if (mark == opponent) opponentCount++;
            else if (mark == Mark::Empty) emptyCount++;
        }

        if (aiCount > 0 && opponentCount > 0) return 0;
        if (aiCount == 3) return scale;
        if (opponentCount == 3) return -scale;
        if (aiCount == 2 && emptyCount == 1) return scale / 3;
        if (opponentCount == 2 && emptyCount == 1) return -scale / 2;
        if (aiCount == 1 && emptyCount == 2) return scale / 18;
        if (opponentCount == 1 && emptyCount == 2) return -scale / 20;
        return 0;
    }

    int macropositionbonus(int row, int col) const {
        if (row == 1 && col == 1) return 80;
        if ((row == 0 || row == 2) && (col == 0 || col == 2)) return 35;
        return 15;
    }

    
};
