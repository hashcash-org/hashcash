package com.ardash.hashcash.test;

import java.security.NoSuchAlgorithmException;

import junit.framework.Assert;

import org.junit.Test;

import com.ardash.hashcash.HashCash;

public class HashCashTest {

	@Test
	public void test() throws NoSuchAlgorithmException {
		//HashCash hc = new HashCash("")
		HashCash hc = HashCash.mintCash("foo@bar", 20,1);
		System.out.println(hc);
//		hc.estimateTime(value)
//		fail("Not yet implemented");
	}
	
	@Test
	public void verifytest() throws NoSuchAlgorithmException {
		HashCash hc = HashCash.parseFromString("1:48:110416:etienne@cri.fr:::000A2F00000063BF012");
		
		System.out.println(hc);
		System.out.println(hc.getValue());
		System.out.println(hc.getHashSumHex());
		System.out.println(hc.isValid());
		
//		hc.estimateTime(value)
//		fail("Not yet implemented");
	}

	@Test
	public void nondeterministicMintAndVerify() {
		for (int j = 0; j < 20; j++) {
			for (int i = 0; i < 20; i++) {
				HashCash hc = HashCash.mintCash("foo@bar", i,1);
				Assert.assertTrue("garbage minted "+ hc, hc.isValid());
			}
		}
//		hc.estimateTime(value)
//		fail("Not yet implemented");
	}

}
