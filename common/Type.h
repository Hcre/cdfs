#ifndef TYPE_H
#define TYPE_H
namespace cdfs{

template <typename To, typename From>
inline To implicit_cast(From const& f) {
    return f;
}
} // namespace cdfs

#endif // TYPE_H