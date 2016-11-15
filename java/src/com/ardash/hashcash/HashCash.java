package com.ardash.hashcash;

/*
 Copyright 2016 Andreas Redmer <a_r@posteo.de>

 Licensed under the Apache License, Version 2.0 (the "License");
 you may not use this file except in compliance with the License.
 You may obtain a copy of the License at

 http://www.apache.org/licenses/LICENSE-2.0

 Unless required by applicable law or agreed to in writing, software
 distributed under the License is distributed on an "AS IS" BASIS,
 WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 See the License for the specific language governing permissions and
 limitations under the License.

 */

import java.util.*;
import java.security.MessageDigest;
import java.security.SecureRandom;
import java.text.SimpleDateFormat;
import java.security.NoSuchAlgorithmException;

/**
 * Hashcat class.
 * 
 * original perl definion of the stamp ver 0
 * 
 * vers:time:resource:trial
 * 
 * original perl definion of the stamp ver 1
 * 
 * ($vers,$bits,$date,$res,$ext,$rand,$ctr)=split(':',$stamp);
 * 
 * my translation:
 * 
 * version:claimedBits:timestamp:resource:extension:randomValue:counter
 * 
 * @author a_r@posteo.de
 * 
 */
public class HashCash implements Comparable<HashCash> {
	public static final int DEFAULT_VERSION = 1;
	private static final int HASH_LENGTH = 160;
	private static final String DATE_FORMAT_V1 = "yyMMdd";
	private static long millisFor16 = -1;

	// make this class unusable if SHA1 is not available
	static {
		try {
			MessageDigest.getInstance("SHA1");
		} catch (NoSuchAlgorithmException e) {
			throw new RuntimeException("SHA1 not available. This class can't"
					+ "be used in this VM implemetation.", e);
		}
	}

	private int version;
	private int claimedBits;
	private int verifiedBits;
	private Calendar date;
	private String resource;
	private String extension;
	private String stamp;
	private byte[] hashSum;

	/**
	 * Parses and validates a HashCash.
	 */
	public static HashCash parseFromString(String hashCashStamp) {
		if (hashCashStamp == null)
			throw new IllegalArgumentException("hashCashStamp must not be null");
		
		HashCash hc = new HashCash();
		hc.stamp = hashCashStamp;
		String[] parts = hashCashStamp.split(":");
		hc.version = Integer.parseInt(parts[0]);
		if (hc.version < 1 || hc.version > 1)
			throw new IllegalArgumentException("Only supported version is 1");

		if ((hc.version == 0 && parts.length != 6)
				|| (hc.version == 1 && parts.length != 7))
			throw new IllegalArgumentException("Improperly formed HashCash");

		try {
			int index = 1;
			hc.claimedBits = Integer.parseInt(parts[index++]);

			SimpleDateFormat dateFormat = new SimpleDateFormat(DATE_FORMAT_V1);
			Calendar tempCal = Calendar
					.getInstance(TimeZone.getTimeZone("GMT"));
			tempCal.setTime(dateFormat.parse(parts[index++]));

			hc.resource = parts[index++];
			hc.extension = parts[index++];

			MessageDigest md = getSafeMessageDigestInstance("SHA1");
			md.update(hashCashStamp.getBytes());
			hc.hashSum = md.digest();
			int zeros = getNumberOfLeadingZeros(hc.hashSum);

			// spec: never confirm more bits as verified, if they were not claimed
			hc.verifiedBits = (zeros > hc.claimedBits ? hc.claimedBits : zeros);
		} catch (java.text.ParseException ex) {
			throw new IllegalArgumentException("Improperly formed HashCash", ex);
		}
		return hc;
	}
	
	private HashCash() {
	}

	private static MessageDigest getSafeMessageDigestInstance(String algorithm) {
		try {
			return MessageDigest.getInstance(algorithm);
		} catch (NoSuchAlgorithmException e) {
			throw new RuntimeException(e);
		}
	}

	private static SecureRandom getSafeSecureRandomInstance(String algorithm) {
		try {
			return SecureRandom.getInstance(algorithm);
		} catch (NoSuchAlgorithmException e) {
			throw new RuntimeException(e);
		}
	}
	
	private static String byteArrayToHexString(byte[] b) {
		  String result = "";
		  for (int i=0; i < b.length; i++) {
		    result +=
		          Integer.toString( ( b[i] & 0xff ) + 0x100, 16).substring( 1 );
		  }
		  return result;
		}

	/**
	 * Mints a version 1 HashCash using now as the date
	 * 
	 * @param resource
	 *            the string to be encoded in the HashCash
	 */
	public static HashCash mintCash(String resource, int value) {
		Calendar now = Calendar.getInstance(TimeZone.getTimeZone("GMT"));
		return mintCash(resource, null, now, value, DEFAULT_VERSION);
	}

	/**
	 * Mints a HashCash using now as the date
	 * 
	 * @param resource
	 *            the string to be encoded in the HashCash
	 * @param version
	 *            Which version to mint. Only valid values are 0 and 1
	 */
	public static HashCash mintCash(String resource, int value, int version) {
		Calendar now = Calendar.getInstance(TimeZone.getTimeZone("GMT"));
		return mintCash(resource, null, now, value, version);
	}

	/**
	 * Mints a version 1 HashCash
	 * 
	 * @param resource
	 *            the string to be encoded in the HashCash
	 */
	public static HashCash mintCash(String resource, Calendar date, int value) {
		return mintCash(resource, null, date, value, DEFAULT_VERSION);
	}

	/**
	 * Mints a HashCash
	 * 
	 * @param resource
	 *            the string to be encoded in the HashCash
	 * @param version
	 *            Which version to mint. Only valid values are 0 and 1
	 */
	public static HashCash mintCash(String resource, Calendar date, int value,
			int version) {
		return mintCash(resource, null, date, value, version);
	}

	/**
	 * Mints a version 1 HashCash using now as the date
	 * 
	 * @param resource
	 *            the string to be encoded in the HashCash
	 * @param extension
	 *            Extra data to be encoded in the HashCash
	 */
	public static HashCash mintCash(String resource, String extension, int value) {
		Calendar now = Calendar.getInstance(TimeZone.getTimeZone("GMT"));
		return mintCash(resource, extension, now, value, DEFAULT_VERSION);
	}

	/**
	 * Mints a HashCash using now as the date
	 * 
	 * @param resource
	 *            the string to be encoded in the HashCash
	 * @param extension
	 *            Extra data to be encoded in the HashCash
	 * @param version
	 *            Which version to mint. Only valid values are 0 and 1
	 */
	public static HashCash mintCash(String resource, String extension,
			int value, int version) {
		Calendar now = Calendar.getInstance(TimeZone.getTimeZone("GMT"));
		return mintCash(resource, extension, now, value, version);
	}

	/**
	 * Mints a version 1 HashCash
	 * 
	 * @param resource
	 *            the string to be encoded in the HashCash
	 * @param extension
	 *            Extra data to be encoded in the HashCash
	 */
	public static HashCash mintCash(String resource, String extension,
			Calendar date, int value) {
		return mintCash(resource, extension, date, value, DEFAULT_VERSION);
	}

	/**
	 * Mints a HashCash
	 * 
	 * @param resource
	 *            the string to be encoded in the HashCash
	 * @param extension
	 *            Extra data to be encoded in the HashCash
	 * @param version
	 *            Which version to mint. Only valid value 1
	 */
	public static HashCash mintCash(String resource, String extension,
			Calendar date, int value, int version) {
		if (version < 1 || version > 1)
			throw new IllegalArgumentException("Only supported version is 1");

		if (value < 0 || value > HASH_LENGTH)
			throw new IllegalArgumentException("Value must be between 0 and "
					+ HASH_LENGTH);

		if (extension == null)
			extension = "";

		if (resource == null || date == null)
			throw new IllegalArgumentException(
					"Resource and date must not be null.");

		if (resource.contains(":"))
			throw new IllegalArgumentException(
					"Resource may not contain a colon.");

		if (extension.contains(":"))
			throw new IllegalArgumentException(
					"Extension may not contain a colon.");

		HashCash result = new HashCash();

		MessageDigest md = getSafeMessageDigestInstance("SHA1");

		result.resource = resource;
		result.extension = (null == extension ? "" : extension);
		result.date = date;
		result.version = version;

		String prefix;

		SimpleDateFormat dateFormat = new SimpleDateFormat(DATE_FORMAT_V1);
		switch (version) {
		case 1:
			result.verifiedBits = value;
			prefix = version + ":" + value + ":"
					+ dateFormat.format(date.getTime()) + ":" + resource + ":"
					+ extension + ":";
			result.stamp = generateCash(prefix, value, md);
			break;

		default:
			throw new IllegalArgumentException(
					"Only supported versions are 0 and 1");
		}

		return result;
	}

	// Accessors
	/**
	 * Two objects are considered equal if they are both of type HashCash and
	 * have an identical string representation
	 */
	public boolean equals(Object obj) {
		if (obj instanceof HashCash)
			return toString().equals(obj.toString());
		else
			return super.equals(obj);
	}

	/**
	 * Returns the canonical string representation of the HashCash
	 */
	public String toString() {
		return stamp;
	}

	/**
	 * Extra data encoded in the HashCash
	 */
	public String getExtension() {
		return extension;
	}

	/**
	 * The primary resource being protected
	 */
	public String getResource() {
		return resource;
	}

	/**
	 * The minting date
	 */
	public Calendar getDate() {
		return date;
	}

	/**
	 * The value of the HashCash (e.g. how many leading zero bits it has).
	 * 
	 * Value accouding to spec:
	 * 
	 * $value = ( $measured_value < $claimed_value ) ? 0 : $claimed_value;
	 */
	public int getValue() {
		return ( verifiedBits < claimedBits ) ? 0 : claimedBits;
	}

	/**
	 * Which version of HashCash is used here
	 */
	public int getVersion() {
		return version;
	}
	
	public byte[] getHashSum()
	{
		return hashSum;
	}

	public String getHashSumHex()
	{
		return byteArrayToHexString(hashSum);
	}

	 public boolean isValid(){
	// // TODO $value = ( $measured_value < $claimed_value ) ? 0 :
	// $claimed_value;
		 // the implementation doesn't work if 0 bits are claimed,
		 // but by spec 0 bits claimed should always be correct
		 return (claimedBits ==0) || (verifiedBits == claimedBits);
	 }

	// Private utility functions
	/**
	 * Actually tries various combinations to find a valid hash. Form is of
	 * prefix + random_hex + ":" + random_hex
	 * 
	 */
	private static String generateCash(String prefix, int value,
			MessageDigest md) {
		SecureRandom rnd = getSafeSecureRandomInstance("SHA1PRNG");
		byte[] tmpBytes = new byte[8];
		rnd.nextBytes(tmpBytes);
		long random = bytesToLong(tmpBytes);
		rnd.nextBytes(tmpBytes);
		long counter = bytesToLong(tmpBytes);

		prefix = prefix + Long.toHexString(random) + ":";

		String temp;
		int tempValue;
		byte[] bArray;
		do {
			counter++;
			temp = prefix + Long.toHexString(counter);
			md.reset();
			md.update(temp.getBytes());
			bArray = md.digest();
			tempValue = getNumberOfLeadingZeros(bArray);
		} while (tempValue < value);

		return temp;
	}

	/**
	 * Converts a 8 byte array of unsigned bytes to an long
	 * 
	 * @param b
	 *            an array of 8 unsigned bytes
	 */
	private static long bytesToLong(byte[] b) {
		long l = 0;
		l |= b[0] & 0xFF;
		l <<= 8;
		l |= b[1] & 0xFF;
		l <<= 8;
		l |= b[2] & 0xFF;
		l <<= 8;
		l |= b[3] & 0xFF;
		l <<= 8;
		l |= b[4] & 0xFF;
		l <<= 8;
		l |= b[5] & 0xFF;
		l <<= 8;
		l |= b[6] & 0xFF;
		l <<= 8;
		l |= b[7] & 0xFF;
		return l;
	}

	/**
	 * Counts the number of leading zeros in a byte array.
	 */
	private static int getNumberOfLeadingZeros(byte[] values) {
		int result = 0;
		int temp = 0;
		for (int i = 0; i < values.length; i++) {
			temp = getNumberOfLeadingZeros(values[i]);
			result += temp;
			if (temp != 8)
				break;
		}
		return result;
	}

	/**
	 * Returns the number of leading zeros in a bytes binary represenation
	 */
	private static int getNumberOfLeadingZeros(byte value) {
		if (value < 0)
			return 0;
		if (value < 1)
			return 8;
		else if (value < 2)
			return 7;
		else if (value < 4)
			return 6;
		else if (value < 8)
			return 5;
		else if (value < 16)
			return 4;
		else if (value < 32)
			return 3;
		else if (value < 64)
			return 2;
		else if (value < 128)
			return 1;
		else
			return 0;
	}

	/**
	 * Estimates how many milliseconds it would take to mint a cash of the
	 * specified value.
	 * <ul>
	 * <li>NOTE1: Minting time can vary greatly in fact, half of the time it
	 * will take half as long)
	 * <li>NOTE2: The first time that an estimation function is called it is
	 * expensive (on the order of seconds). After that, it is very quick.
	 * </ul>
	 */
	public static long estimateTime(int value) {
		initEstimates();
		return (long) (millisFor16 * Math.pow(2, value - 16));
	}

	/**
	 * Estimates what value (e.g. how many bits of collision) are required for
	 * the specified length of time.
	 * <ul>
	 * <li>NOTE1: Minting time can vary greatly in fact, half of the time it
	 * will take half as long)
	 * <li>NOTE2: The first time that an estimation function is called it is
	 * expensive (on the order of seconds). After that, it is very quick.
	 * </ul>
	 */
	public static int estimateValue(int secs) {
		initEstimates();
		int result = 0;
		long millis = secs * 1000 * 65536;
		millis /= millisFor16;

		while (millis > 1) {
			result++;
			millis /= 2;
		}

		return result;
	}

	/**
	 * Seeds the estimates by determining how long it takes to calculate a 16bit
	 * collision on average.
	 */
	private static void initEstimates() {
		if (millisFor16 <=0 ) {
			mintCash("estimation", 16); // one mint done unmeasured to avoid optimisations
			long duration = Calendar.getInstance().getTimeInMillis();
			for (int i = 0; i < 11; i++) {
				mintCash("estimation", 16);
			}
			duration = Calendar.getInstance().getTimeInMillis() - duration;
			millisFor16 = (duration / 10); // TODO wrong! remove /10
		}
	}

	/**
	 * Compares the value of two HashCashes
	 * 
	 * @param other
	 * @see java.lang.Comparable#compareTo(Object)
	 */
	public int compareTo(HashCash other) {
		if (null == other)
			throw new NullPointerException();

		return Integer.valueOf(getValue()).compareTo(
				Integer.valueOf(other.getValue()));
	}
}