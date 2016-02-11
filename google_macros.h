#ifndef GOOGLE_MACROS_H_
#define GOOGLE_MACROS_H_

#undef GOOGLE_DISALLOW_EVIL_CONSTRUCTORS
#define GOOGLE_DISALLOW_EVIL_CONSTRUCTORS(TypeName) \
      TypeName(const TypeName&);                        \
      void operator=(const TypeName&)

#endif  // GOOGLE_MACROS_H_
