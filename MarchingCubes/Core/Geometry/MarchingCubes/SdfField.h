#pragma once
#include <cstdint>
#include <cstring>
#include <memory>
#include <type_traits>
#include <stdexcept>

#if defined(_MSC_VER)
#include <malloc.h> 
#endif
// 32B 정렬 해제를 보장하는 deleter
struct AlignedDeleter {
    void operator()(void* p) const noexcept {
#if defined(_MSC_VER)
        _aligned_free(p);
#else
        free(p);
#endif
    }
};

// 32B 정렬된 배열을 unique_ptr로 생성
template <typename T>
inline std::unique_ptr<T, AlignedDeleter>
make_aligned_array(std::size_t n, std::size_t align = 32) {
    if (n == 0) return { nullptr, AlignedDeleter{} };
    void* p = nullptr;
#if defined(_MSC_VER)
    p = _aligned_malloc(n * sizeof(T), align);
    if (!p) throw std::bad_alloc();
#else
    if (posix_memalign(&p, align, n * sizeof(T)) != 0) throw std::bad_alloc();
#endif
    return std::unique_ptr<T, AlignedDeleter>(static_cast<T*>(p));
}

// 연속 1D 그리드 컨테이너 (X-최내부, 32B 정렬)
template <typename T = float>
class SdfField {
public:
    using value_type = T;

    // 생성/소멸/이동
    SdfField() = default;
    SdfField(int sx, int sy, int sz) { allocate(sx, sy, sz); }

    SdfField(const SdfField&) = delete;
    SdfField& operator=(const SdfField&) = delete;

    SdfField(SdfField&& rhs) noexcept { moveFrom(std::move(rhs)); }
    SdfField& operator=(SdfField&& rhs) noexcept {
        if (this != &rhs) { release(); moveFrom(std::move(rhs)); }
        return *this;
    }

    ~SdfField() { release(); }

    // 크기/상태
    int  sx() const noexcept { return Sx_; }
    int  sy() const noexcept { return Sy_; }
    int  sz() const noexcept { return Sz_; }
    bool empty() const noexcept { return data_.get() == nullptr; }
    std::size_t size() const noexcept {
        return data_ ? static_cast<std::size_t>(Sx_) * Sy_ * Sz_ : 0;
    }
    explicit operator bool() const noexcept { return !empty(); }

    // 메모리 관리 ------------------------------------------------------------
    void allocate(int sx, int sy, int sz) {
        release();
        if (sx <= 0 || sy <= 0 || sz <= 0)
            throw std::invalid_argument("SdfField::allocate: invalid size");
        Sx_ = sx; Sy_ = sy; Sz_ = sz;

        const std::size_t n = static_cast<std::size_t>(sx) * sy * sz;
        data_ = make_aligned_array<T>(n, 32);  // 32-Bytes 정렬
        buildPointerTables();
    }

    void release() noexcept {
        data_.reset();    // RAII로 안전 해제
        Sx_ = Sy_ = Sz_ = 0;
        rowPtrs_.clear();
        zPtrs_.clear();
    }

    // 전체 채우기: field = 0.0f; (원소 단위로 채움)
    SdfField& operator=(const T& v) noexcept {
        if (!data_) return *this;
        T* p = data_.get();
        const std::size_t n = size();
        for (std::size_t i = 0; i < n; ++i) p[i] = v;
        return *this;
    }

    // 데이터/포인터 ----------------------------------------------------------
    inline       T* data()       noexcept { return data_.get(); }
    inline const T* data() const noexcept { return data_.get(); }

    // 선형 인덱스 (X-연속)
    static inline std::size_t idx_linear(int x, int y, int z, int Sx, int Sy) noexcept {
        return static_cast<std::size_t>(z) * static_cast<std::size_t>(Sy) * static_cast<std::size_t>(Sx)
            + static_cast<std::size_t>(y) * static_cast<std::size_t>(Sx)
            + static_cast<std::size_t>(x);
    }
    inline std::size_t idx(int x, int y, int z) const noexcept {
        return idx_linear(x, y, z, Sx_, Sy_);
    }
    // 클램프된 선형 인덱스(삼항 연산자)
    inline std::size_t idx_clamed(int x, int y, int z) const noexcept {
        x = (x < 0) ? 0 : ((x >= Sx_) ? (Sx_ - 1) : x);
        y = (y < 0) ? 0 : ((y >= Sy_) ? (Sy_ - 1) : y);
        z = (z < 0) ? 0 : ((z >= Sz_) ? (Sz_ - 1) : z);
        return idx_linear(x, y, z, Sx_, Sy_);
    }

    // 명시적 접근
    inline       T& at(int x, int y, int z)       noexcept { return data_.get()[idx(x, y, z)]; }
    inline const T& at(int x, int y, int z) const noexcept { return data_.get()[idx(x, y, z)]; }

    // 클램프 접근
    inline       T& at_clamped(int x, int y, int z)       noexcept { return data_.get()[idx_clamed(x, y, z)]; }
    inline const T& at_clamped(int x, int y, int z) const noexcept { return data_.get()[idx_clamed(x, y, z)]; }

    // 행 포인터(X-연속 → SIMD 친화)
    inline       T* rowPtr(int y, int z)       noexcept { return data_.get() + idx(0, y, z); }
    inline const T* rowPtr(int y, int z) const noexcept { return data_.get() + idx(0, y, z); }

    // 3중 대괄호 접근: field[z][y][x]
    struct YProxy {
        SdfField* f; int y; int z;
        inline       T& operator[](int x)       noexcept { return f->at(x, y, z); }
        inline const T& operator[](int x) const noexcept { return f->at(x, y, z); }
    };
    struct ZProxy {
        SdfField* f; int z;
        inline YProxy operator[](int y) noexcept { return YProxy{ f, y, z }; }
        inline const YProxy operator[](int y) const noexcept { return YProxy{ const_cast<SdfField*>(f), y, z }; }
    };
    inline ZProxy operator[](int z) noexcept { return ZProxy{ this, z }; }
    inline const ZProxy operator[](int z) const noexcept { return ZProxy{ const_cast<SdfField*>(this), z }; }

    explicit operator T*** () noexcept {
        return zPtrs_.empty() ? nullptr : zPtrs_.data();
    }
    explicit operator const T* const* const* () const noexcept {
        return zPtrs_.empty()
            ? nullptr
            : reinterpret_cast<const T* const* const*>(zPtrs_.data());
    }

    // 재할당 없이 포인터 테이블만 재구성하고 싶을 때
    void rebuildTriplePtr() { buildPointerTables(); }

private:
    int Sx_{ 0 }, Sy_{ 0 }, Sz_{ 0 };
    std::unique_ptr<T, AlignedDeleter> data_{ nullptr, AlignedDeleter{} };

    // 포인터 테이블 (데이터 복사 없음: 행/슬라이스 포인터만 저장)
    std::vector<T*>  rowPtrs_;  // [Sz*Sy] : 각 (z,y)의 행 시작 포인터 &data_[z,y,0]
    std::vector<T**> zPtrs_;    // [Sz]    : 각 z의 행 배열 시작 주소 &rowPtrs_[z*Sy]

    void moveFrom(SdfField&& r) noexcept {
        Sx_ = r.Sx_; Sy_ = r.Sy_; Sz_ = r.Sz_;
        data_ = std::move(r.data_);
        rowPtrs_ = std::move(r.rowPtrs_);
        zPtrs_ = std::move(r.zPtrs_);
        r.Sx_ = r.Sy_ = r.Sz_ = 0;
    }

    void buildPointerTables() {
        if (!data_) { rowPtrs_.clear(); zPtrs_.clear(); return; }
        rowPtrs_.resize(static_cast<std::size_t>(Sy_) * static_cast<std::size_t>(Sz_));
        zPtrs_.resize(static_cast<std::size_t>(Sz_));
        for (int z = 0; z < Sz_; ++z) {
            T** rows = &rowPtrs_[static_cast<std::size_t>(z) * static_cast<std::size_t>(Sy_)];
            zPtrs_[static_cast<std::size_t>(z)] = rows;
            for (int y = 0; y < Sy_; ++y) rows[static_cast<std::size_t>(y)] = rowPtr(y, z);
        }
    }
};