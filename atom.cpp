// [x] fragmentation
// [x] reassembly
// [x] de/multiplexing
// [x] channels
// (recovery)
// (checksum)

#include <string>
#include <vector>
#include <set>

struct fragment {
    unsigned id, seq, left;
    std::string payload;

    unsigned total() const {
        return left + seq;
    }

    bool operator <( const fragment &other ) const {
        return id != other.id ? id < other.id : seq < other.seq;
    }

    template<typename ostream>
    friend ostream &operator <<( ostream &os, const fragment &s ) {
        os << "(" << s.id << "," << s.seq << "/" << s.total() << "," << s.payload << ")";
        return os;
    }
};

struct fragments : public std::vector<fragment> {

    fragments &operator +=( const fragments &other ) {
        for( auto &f : other ) {
            this->push_back( f );
        }
        return *this;
    }
    fragments operator +( const fragments &other ) const {
        fragments fs = other;
        return fs += *this;
    }

    template<typename ostream>
    friend ostream &operator <<( ostream &os, const fragments &fs ) {
        os << "[";
        for( auto &f : fs ) {
            os << f;
        }
        os << "]";
        return os;
    }
};

#include <iostream>
#include <map>
fragments sort( const fragments &fs ) {
    std::set<fragment> unique_and_sorted( fs.begin(), fs.end() );
    fragments ffs;
    for( auto &f: unique_and_sorted ) {
        ffs.push_back( f );
    }
    return ffs;
}
std::map<unsigned/*id*/,unsigned/*misses*/> integrity( const fragments &fs ) {
    std::map<unsigned,unsigned> hits;
    auto nodupes = sort( fs );
    for( auto &f : nodupes ) {
        if( hits.find(f.id) == hits.end() ) {
            hits[f.id] = f.total() - 1;
        } else {
            hits[f.id]--;
        }
    }
    return hits;
}
bool eof( const fragments &fs, unsigned id ) {
    auto ret = integrity( fs );
    auto found = ret.find(id);
    if( found == ret.end() ) {
        return false;
    }
    return found->second == 0;
}
fragments split( const std::string &data, unsigned bytes = 48, unsigned id = 0 ) {
    unsigned seq = 0, left = bytes;
    fragments fs;
    fragment f { id, seq++, 0 };
    for( const auto &ch : data ) {
        f.payload += ch;
        if( !--left ) {
            left = bytes;
            fs.push_back( f );
            f = fragment { id, seq++, 0 };
        }
    }
    if( !f.payload.empty() ) {
        fs.push_back( f );
    }
    for( auto &f : fs ) {
        f.left = fs.size() - f.seq;
    }
    return fs;
}
fragments split( const std::vector<std::string> &data, unsigned bytes = 48 ) {
    fragments fs;
    unsigned id = 0;
    for( auto &d : data ) {
        fs += split( d, bytes, id++ );
    }
    return fs;
}
#include <map>
#include <iostream>
std::map<unsigned,std::string> joins( const fragments &fragments ) {
    std::map<unsigned,std::string> payloads;
    auto nodupes = sort( fragments );
    for( const auto &f : nodupes ) {
        payloads[f.id] += f.payload;
    }
    for( const auto &f : payloads ) {
        const auto &id = f.first;
        if( !eof(nodupes, id) ) {
            payloads[id].clear();            
        }
    }
    return payloads;
}
std::string join( const fragments &fragments, unsigned id = 0 ) {
    return ::joins( fragments )[id];
}

#include <cassert>

#include <algorithm>
#include <random>
#include <chrono>

int main() {
    std::vector<std::string> original = {
      "lorem ipsum dolor and etcetera...",
      "abc def ghi jkl -- 1 2 3 4 5"
    };

    auto reassemble = [&]( fragments fs, unsigned expected ) {
        std::cout << fs << std::endl;
        auto rebuilt0 = join( fs, 0 );
        auto rebuilt1 = join( fs, 1 );
        std::cout << rebuilt0 << std::endl;
        std::cout << rebuilt1 << std::endl;
        if( expected == 2 ) {
            assert( original[0] == rebuilt0 );
            assert( original[1] == rebuilt1 );
        }
        if( expected == 1 ) {
            assert( original[0] == rebuilt0 || original[1] == rebuilt1 );
        }
        if( expected == 0 ) {
            assert( original[0] != rebuilt0 );
            assert( original[1] != rebuilt1 );
        }
    };

    auto shuffle = [&]( fragments fs ) -> fragments {
        unsigned seed = std::chrono::system_clock::now().time_since_epoch().count();
        std::shuffle (fs.begin(), fs.end(), std::default_random_engine(seed));
        return fs;
    };

    auto corrupt = [&]( fragments fs, unsigned packets ) -> fragments {
        fs = shuffle( fs ); 
        while( packets-- ) {
            fs.erase( fs.begin() );
        }
        return fs;
    };

    auto fragments = split(original, 3);

    if( const bool test_eof = true ) {
        auto with = fragments;
        for( const auto &pair : integrity( with ) ) {
            const auto &channel = pair.first;
            const auto &eof = pair.second;
            std::cout << "ch #" << channel << ", integrity: " << eof << ", eof: " << ::eof(with, channel) << std::endl;
        }

        with = corrupt( fragments, 3 );
        for( const auto &pair : integrity( with ) ) {
            const auto &channel = pair.first;
            const auto &eof = pair.second;
            std::cout << "ch #" << channel << ", integrity: " << eof << ", eof: " << ::eof(with, channel) << std::endl;
        }
    }

    if( const bool test_reassembly = true ) {
        reassemble( fragments, 2 );
    }

    if( const bool test_shuffle = true ) {
        reassemble( shuffle(fragments), 2 );
    }

    if( const bool test_corruption = true ) {
        reassemble( corrupt(fragments, 1), 1 );
    }

    std::cout << "All ok." << std::endl;
}
