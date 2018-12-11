//PhoneNumberLineEdit.h by Rngcoin developers
#include <QLineEdit>

struct PhoneNumberLineEdit: public QLineEdit {
	PhoneNumberLineEdit();
	QString toPhoneNumber()const;
};
